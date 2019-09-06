// The MIT License (MIT)
//
// Copyright (c) 2016-2019 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <iostream>
#include <thread>

#include <daw/daw_scope_guard.h>
#include <daw/parallel/daw_latch.h>

#include "daw/fs/impl/ithread.h"
#include "daw/fs/task_scheduler.h"

namespace daw {
	task_scheduler get_task_scheduler( ) {
		static auto ts = []( ) {
			auto result = task_scheduler( );
			result.start( );
			return result;
		}( );
		if( not ts ) {
			ts.start( );
		}
		return ts;
	}

	namespace {
		template<typename... Args>
		::daw::parallel::ithread create_thread( Args &&... args ) noexcept {
			try {
				return ::daw::parallel::ithread( std::forward<Args>( args )... );
			} catch( std::system_error const &e ) {
				std::cerr << "Error creating thread, aborting.\n Error code: "
				          << e.code( ) << "\nMessage: " << e.what( ) << std::endl;
				std::abort( );
			}
		}
	} // namespace

	task_scheduler::task_scheduler_impl::task_scheduler_impl(
	  std::size_t num_threads, bool block_on_destruction )
	  : m_num_threads( num_threads )
	  , m_tasks( make_task_queues( num_threads ) )
	  , m_block_on_destruction( block_on_destruction ) {}

	task_scheduler::task_scheduler( ) {
		start( );
	}

	task_scheduler::task_scheduler( std::size_t num_threads )
	  : m_impl( new task_scheduler_impl( num_threads, true ) ) {
		start( );
	}

	task_scheduler::task_scheduler( std::size_t num_threads,
	                                bool block_on_destruction )
	  : m_impl( new task_scheduler_impl( num_threads, block_on_destruction ) ) {

		start( );
	}

	task_scheduler::task_scheduler( std::size_t num_threads,
	                                bool block_on_destruction, bool auto_start )
	  : m_impl( new task_scheduler_impl( num_threads, block_on_destruction ) ) {

		if( auto_start ) {
			start( );
		}
	}

	task_scheduler::~task_scheduler( ) {
		if( auto tmp = std::exchange( m_impl, nullptr ); tmp ) {
			stop( tmp->m_block_on_destruction );
		}
	}

	bool task_scheduler::started( ) const {
		return m_impl->m_continue;
	}

	::daw::task_t task_scheduler::wait_for_task_from_pool( size_t id ) {
		// Get task.  First try own queue, if not try the others and finally
		// wait for own queue to fill
		if( not m_impl or not m_impl->m_continue ) {
			return {};
		}
		if( auto tsk = m_impl->m_tasks[id].try_pop_front( ); tsk ) {
			return ::daw::move( tsk );
		}
		for( auto m = ( id + 1 ) % m_impl->m_num_threads;
		     m_impl->m_continue and m != id;
		     m = ( m + 1 ) % m_impl->m_num_threads ) {

			if( auto tsk = m_impl->m_tasks[m].try_pop_front( ); tsk ) {
				return ::daw::move( tsk );
			}
		}
		return m_impl->m_tasks[id].pop_front(
		  [&]( ) { return static_cast<bool>( m_impl->m_continue ); } );
	}

	void task_scheduler::run_task( ::daw::task_t &&tsk ) noexcept {
		if( not tsk ) {
			return;
		}
		try {
			if( not m_impl->m_continue ) {
				return;
			}
			if( tsk.is_ready( ) ) {
				(void)send_task( ::daw::move( tsk ), get_task_id( ) );
			} else {
				(void)daw::move( tsk )( );
			}
		} catch( ... ) {
			daw::breakpoint( );
			// Don't let a task take down thread
			// TODO: add callback to task_scheduler for handling
			// task exceptions
		}
	}

	size_t task_scheduler::get_task_id( ) {
		auto const tc = m_impl->m_task_count++;
		return tc % m_impl->m_num_threads;
	}

	bool task_scheduler::run_next_task( size_t id ) {
		if( auto tsk = m_impl->m_tasks[id].try_pop_front( ); tsk ) {
			run_task( daw::move( tsk ) );
			return true;
		}
		for( auto m = ( id + 1 ) % m_impl->m_num_threads; m != id;
		     m = ( m + 1 ) % m_impl->m_num_threads ) {

			if( auto tsk = m_impl->m_tasks[m].try_pop_front( ); tsk ) {
				run_task( daw::move( tsk ) );
				return true;
			}
		}
		return false;
	}

	void task_scheduler::start( ) {
		if( started( ) ) {
			return;
		}
		m_impl->m_continue = true;
		auto threads = m_impl->m_threads.get( );
		auto thread_map = m_impl->m_thread_map.get( );
		for( size_t n = 0; n < m_impl->m_num_threads; ++n ) {
			auto thr = create_thread(
			  []( size_t id, auto wself ) {
				  auto self = wself.lock( );
				  if( !self ) {
					  return;
				  }
				  self->task_runner( id );
			  },
			  n, get_handle( ) );
			auto id = thr.get_id( );
			threads->push_back( ::daw::move( thr ) );
			( *thread_map )[id] = n;
		}
	}

	void task_scheduler::stop( bool block ) noexcept {
		if( auto tmp = m_impl; tmp ) {
			try {
				auto threads = tmp->m_threads.get( );
				for( auto &th : *threads ) {
					try {
						if( block ) {
							th.stop_and_wait( );
						} else {
							th.detach( );
							th.stop( );
						}
					} catch( ... ) {}
				}
				threads->clear( );
			} catch( ... ) {}
		}
	}

	daw::shared_latch
	task_scheduler::start_temp_task_runners( size_t task_count ) {
		auto sem = daw::shared_latch( task_count );
		auto wself = get_handle( );
		using wself_t = decltype( wself );
		// *****
		struct tmp_worker {
			mutable typename ::std::list<::daw::parallel::ithread>::iterator m_it;
			size_t m_id;
			wself_t m_wself;
			mutable ::daw::shared_latch m_sem;

			inline void
			operator( )( ::daw::parallel::interrupt_token /* can_continue */ ) const {
				auto self = m_wself.lock( );
				if( not self ) {
					return;
				}
				auto const at_exit = daw::on_scope_exit(
				  [&]( ) { self->m_impl->m_other_threads.get( )->erase( m_it ); } );

				self->task_runner( m_id, m_sem );
			}
		};
		// *****
		for( size_t n = 0; n < task_count; ++n ) {
			size_t const id = m_impl->m_current_id++;

			auto other_threads = m_impl->m_other_threads.get( );
			auto it = other_threads->emplace( other_threads->end( ) );
			*it = create_thread( tmp_worker{it, id, get_handle( ), sem} );
			it->detach( );
			// other_threads.release( );
		}
		return sem;
	}

	bool task_scheduler::send_task( ::daw::task_t &&tsk, size_t id ) {
		if( !tsk ) {
			return true;
		}
		if( not m_impl->m_continue ) {
			return true;
		}
		if( m_impl->m_tasks[id].try_push_back( ::daw::move( tsk ) ) ==
		    daw::parallel::push_back_result::success ) {
			return true;
		}
		for( auto m = ( id + 1 ) % m_impl->m_num_threads; m != id;
		     m = ( m + 1 ) % m_impl->m_num_threads ) {
			if( not m_impl->m_continue ) {
				return true;
			}
			if( m_impl->m_tasks[m].try_push_back( ::daw::move( tsk ) ) ==
			    daw::parallel::push_back_result::success ) {
				return true;
			}
		}
		return m_impl->m_tasks[id].push_back( ::daw::move( tsk ), [&]( ) {
			return static_cast<bool>( m_impl->m_continue );
		} );
	}

	void task_scheduler::task_runner( size_t id ) {
		auto self = *get_handle( ).lock( );
		while( self.m_impl->m_continue ) {
			run_task( self.wait_for_task_from_pool( id ) );
		}
	}

	void task_scheduler::task_runner( size_t id, daw::shared_latch &sem ) {
		while( m_impl->m_continue and not sem.try_wait( ) ) {
			run_task( wait_for_task_from_pool( id ) );
		}
	}

	char const *unable_to_add_task_exception::what( ) const noexcept {
		return "Unable to add task";
	}
} // namespace daw
