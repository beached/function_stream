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
		template<typename Callable, typename... Args>
		::daw::parallel::ithread create_thread( Callable &&callable,
		                                        Args &&... args ) noexcept {
			try {
				return ::daw::parallel::ithread( std::forward<Callable>( callable ),
				                                 std::forward<Args>( args )... );
			} catch( std::system_error const &e ) {
				std::cerr << "Error creating thread, aborting.\n Error code: "
				          << e.code( ) << "\nMessage: " << e.what( ) << '\n';
				std::abort( );
			}
		}
	} // namespace

	task_scheduler::task_scheduler_impl::task_scheduler_impl(
	  std::size_t num_threads, bool block_on_destruction )
	  : m_num_threads( num_threads )
	  , m_tasks( )
	  , m_block_on_destruction( block_on_destruction ) {

		m_tasks.resize( m_num_threads );
		std::cout << m_tasks.size( ) << '\n';
	}

	task_scheduler::task_scheduler( ) {
		start( );
	}

	task_scheduler::task_scheduler( std::size_t num_threads,
	                                bool block_on_destruction )
	  : m_impl( new task_scheduler_impl( num_threads, block_on_destruction ) ) {

		start( );
	}

	task_scheduler::task_scheduler_impl::~task_scheduler_impl( ) {
		stop( m_block_on_destruction );
	}

	bool task_scheduler::started( ) const {
		assert( m_impl );
		return m_impl->m_continue;
	}

	::daw::task_t task_scheduler::wait_for_task_from_pool( size_t id ) {
		// Get task.  First try own queue, if not try the others and finally
		// wait for own queue to fill
		assert( m_impl );
		if( not m_impl or not m_impl->m_continue ) {
			return {};
		}
		if( id < m_impl->m_tasks.size( ) ) {
			if( auto tsk = m_impl->m_tasks[id].try_pop_front( ); tsk ) {
				return tsk;
			}
		}
		for( size_t n = id + 1; n < std::size( m_impl->m_tasks ); ++n ) {
			if( auto tsk = m_impl->m_tasks[n].try_pop_front( ); tsk ) {
				return tsk;
			}
		}
		for( size_t n = 0; n < id; ++n ) {
			if( auto tsk = m_impl->m_tasks[n].try_pop_front( ); tsk ) {
				return tsk;
			}
		}
		/*
		for( auto m = ( id + 1 ) % m_impl->m_num_threads;
		     m_impl->m_continue and m != id;
		     m = ( m + 1 ) % m_impl->m_num_threads ) {

		  if( auto tsk = m_impl->m_tasks[m].try_pop_front( ); tsk ) {
		    return ::daw::move( tsk );
		  }
		}
		 */
		return pop_front( m_impl->m_tasks[id], [&]( ) {
			return static_cast<bool>( m_impl->m_continue );
		} );
	}

	[[nodiscard]] ::daw::task_t
	task_scheduler::wait_for_task_from_pool( size_t id,
	                                         ::daw::shared_latch sem ) {
		assert( m_impl );
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
		return pop_front( m_impl->m_tasks[id], [&]( ) {
			return static_cast<bool>( m_impl->m_continue && not sem.try_wait( ) );
		} );
	}

	void task_scheduler::run_task( ::daw::task_t &&tsk ) noexcept {
		assert( m_impl );
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
		for( size_t n = id + 1; n < std::size( m_impl->m_tasks ); ++n ) {
			if( auto tsk = m_impl->m_tasks[n].try_pop_front( ); tsk ) {
				run_task( ::daw::move( tsk ) );
				return true;
			}
		}
		for( size_t n = 0; n < id; ++n ) {
			if( auto tsk = m_impl->m_tasks[n].try_pop_front( ); tsk ) {
				run_task( ::daw::move( tsk ) );
				return true;
			}
		}
		/*
		for( auto m = ( id + 1 ) % m_impl->m_num_threads; m != id;
		     m = ( m + 1 ) % m_impl->m_num_threads ) {

		  if( auto tsk = m_impl->m_tasks[m].try_pop_front( ); tsk ) {
		    run_task( daw::move( tsk ) );
		    return true;
		  }
		}
		 */
		return false;
	}

	void task_scheduler::add_queue( size_t n ) {
		assert( m_impl );
		auto threads = m_impl->m_threads.get( );
		auto thread_map = m_impl->m_thread_map.get( );
		auto thr = create_thread(
		  []( size_t id, auto wself ) {
			  auto self = wself.lock( );
			  if( not self ) {
				  return;
			  }
			  self->task_runner( id );
		  },
		  n, get_handle( ) );
		auto id = thr.get_id( );
		threads->push_back( ::daw::move( thr ) );
		( *thread_map )[id] = n;
	}

	void task_scheduler::start( ) {
		assert( m_impl );
		if( started( ) ) {
			return;
		}
		m_impl->m_continue = true;
		// assert( m_impl->m_tasks.size( ) == m_impl->m_num_threads );
		for( size_t n = 0; n < m_impl->m_num_threads; ++n ) {
			add_queue( n );
		}
	}

	void task_scheduler::task_scheduler_impl::stop( bool block_on_destruction ) {
		m_continue = false;
		try {
			auto threads = m_threads.get( );
			for( auto &th : *threads ) {
				try {
					if( block_on_destruction ) {
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

	void task_scheduler::stop( bool block ) noexcept {
		m_impl->stop( block );
	}

	task_scheduler::temp_task_runner task_scheduler::start_temp_task_runner( ) {
		auto sem = daw::shared_latch( );
		auto wself = get_handle( );
		using wself_t = decltype( wself );
		// *****
		struct tmp_worker {
			size_t m_id;
			wself_t m_wself;
			mutable ::daw::shared_latch m_sem;

			inline void operator( )( ) const {
				auto self = m_wself.lock( );
				if( not self ) {
					return;
				}
				self->task_runner( m_id, m_sem );
			}
		};
		// *****
		size_t const id = m_impl->m_current_id++;

		return {create_thread( tmp_worker{id, get_handle( ), sem} ),
		        ::daw::move( sem )};
	}

	bool task_scheduler::send_task( ::daw::task_t &&tsk, size_t id ) {
		if( not tsk ) {
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
		return push_back( m_impl->m_tasks[id], ::daw::move( tsk ), [&]( ) {
			       return static_cast<bool>( m_impl->m_continue );
		       } ) == ::daw::parallel::push_back_result::success;
	}

	void task_scheduler::task_runner( size_t id ) {
		auto w_self = get_handle( );
		while( true ) {
			auto tsk = task_t( );
			{
				auto self = w_self.lock( );
				if( not self or not self->m_impl->m_continue ) {
					return;
				}
				tsk = self->wait_for_task_from_pool( id );
			}
			run_task( ::daw::move( tsk ) );
		}
	}

	void task_scheduler::task_runner( size_t id, daw::shared_latch &sem ) {
		auto w_self = get_handle( );
		while( not sem.try_wait( ) ) {
			auto tsk = task_t( );
			{
				auto self = w_self.lock( );
				if( not self or not self->m_impl->m_continue or not sem.try_wait( ) ) {
					return;
				}
				tsk = self->wait_for_task_from_pool( id, sem );
			}
			run_task( ::daw::move( tsk ) );
		}
	}

	bool task_scheduler::has_empty_queue( ) const {
		for( auto const &q : m_impl->m_tasks ) {
			if( q.empty( ) ) {
				return true;
			}
		}
		return false;
	}

	char const *unable_to_add_task_exception::what( ) const noexcept {
		return "Unable to add task";
	}
} // namespace daw
