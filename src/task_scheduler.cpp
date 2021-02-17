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

#include <daw/daw_scope_guard.h>

#include "daw/fs/impl/daw_latch.h"
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
		/*
		template<typename Callable, typename... Args>
		std::unique_ptr<daw::parallel::ithread> create_thread( Callable &&callable,
		                                                       Args &&...args ) {
		  try {
		    return std::make_unique<daw::parallel::ithread>( DAW_FWD( callable ),
		                                                     DAW_FWD( args )... );
		  } catch( std::system_error const &e ) {
		    std::cerr << "Error creating thread, aborting.\n Error code: "
		              << e.code( ) << "\nMessage: " << e.what( ) << '\n';
		    std::terminate( );
		  }
		}*/
	} // namespace

	task_scheduler::task_scheduler_impl::task_scheduler_impl(
	  std::size_t num_threads, bool block_on_destruction )
	  : m_num_threads( num_threads )
	  , m_tasks( m_num_threads )
	  , m_block_on_destruction( block_on_destruction ) {

		std::cout << std::size( m_tasks ) << '\n';
	}

	task_scheduler::task_scheduler( ) {
		start( );
	}

	task_scheduler::task_scheduler( std::size_t num_threads,
	                                bool block_on_destruction )
	  : m_impl( make_ts( num_threads, block_on_destruction ) ) {

		start( );
	}

	task_scheduler::task_scheduler_impl::~task_scheduler_impl( ) {
		stop( m_block_on_destruction );
	}

	bool task_scheduler::started( ) const {
		assert( m_impl );
		return m_impl->m_continue;
	}

	std::unique_ptr<daw::task_t>
	task_scheduler::wait_for_task_from_pool( size_t id ) {
		// Get task.  First try own queue, if not try the others and finally
		// wait for own queue to fill
		assert( m_impl );
		if( not m_impl or not m_impl->m_continue ) {
			return { };
		}
		std::size_t const queue_count = std::size( m_impl->m_tasks );
		std::size_t const q_id = id % queue_count;
		for( size_t n = q_id; n < queue_count; ++n ) {
			if( auto tsk = m_impl->m_tasks[n].try_pop_front( ); tsk ) {
				return tsk;
			}
		}
		for( size_t n = 0; n < q_id; ++n ) {
			if( auto tsk = m_impl->m_tasks[n].try_pop_front( ); tsk ) {
				return tsk;
			}
		}
		if( id >= queue_count ) {
			return m_impl->m_tasks[q_id].try_pop_front( );
		}
		return pop_front( m_impl->m_tasks[q_id], [&]( ) {
			bool result = m_impl->m_continue.load( std::memory_order_acquire );
			return result;
		} );
	}

	[[nodiscard]] std::unique_ptr<daw::task_t>
	task_scheduler::wait_for_task_from_pool( size_t id,
	                                         daw::parallel::stop_token tok ) {
		assert( m_impl );
		// Get task.  First try own queue, if not try the others and finally
		// wait for own queue to fill
		if( not m_impl or not m_impl->m_continue or not tok ) {
			return { };
		}
		std::size_t const sz = std::size( m_impl->m_tasks );
		std::size_t qid = id % sz;
		bool const is_tmp_worker = id != qid;

		for( auto m = ( qid + 1 ) % sz; m_impl->m_continue and m != qid and tok;
		     m = ( m + 1 ) % sz ) {

			if( auto tsk = m_impl->m_tasks[m].try_pop_front( ); tsk ) {
				if( not tok or not m_impl->m_continue ) {
					return { };
				}
				return tsk;
			}
		}
		if( is_tmp_worker ) {
			while( m_impl->m_continue and tok ) {
				if( auto tsk = m_impl->m_tasks[qid].try_pop_front( ); tsk ) {
					if( not tok or not m_impl->m_continue ) {
						return { };
					}
					return tsk;
				}
				qid = ( qid + 1 ) % sz;
			}
			return { };
		} else {
			return pop_front( m_impl->m_tasks[qid],
			                  [&]( ) { return m_impl->m_continue and tok; } );
		}
	}

	[[nodiscard]] std::unique_ptr<daw::task_t>
	task_scheduler::wait_for_task_from_pool( size_t id, daw::shared_latch sem ) {
		assert( m_impl );
		// Get task.  First try own queue, if not try the others and finally
		// wait for own queue to fill
		if( not m_impl or not m_impl->m_continue ) {
			return nullptr;
		}
		if( auto tsk = m_impl->m_tasks[id].try_pop_front( ); tsk ) {
			return tsk;
		}
		for( auto m = ( id + 1 ) % m_impl->m_num_threads;
		     m_impl->m_continue and m != id;
		     m = ( m + 1 ) % m_impl->m_num_threads ) {

			if( auto tsk = m_impl->m_tasks[m].try_pop_front( ); tsk ) {
				return tsk;
			}
		}
		return pop_front( m_impl->m_tasks[id], [&]( ) {
			return static_cast<bool>( m_impl->m_continue and not sem.try_wait( ) );
		} );
	}

	void task_scheduler::run_task( std::unique_ptr<daw::task_t> &&tsk_ptr ) {
		assert( m_impl );
		if( not tsk_ptr ) {
			return;
		}
		auto &tsk = *tsk_ptr;
		if( not tsk ) {
			return;
		}
		try {
			if( not m_impl->m_continue ) {
				return;
			}
			if( tsk.is_ready( ) ) {
				(void)send_task( daw::move( tsk_ptr ), get_task_id( ) );
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
		assert( m_impl );
		auto const tc = m_impl->m_task_count++;
		// return tc % m_impl->m_num_threads;
		return tc % std::size( m_impl->m_tasks );
	}

	bool task_scheduler::run_next_task( size_t id ) {
		assert( m_impl );
		if( auto tsk = m_impl->m_tasks[id].try_pop_front( ); tsk ) {
			run_task( daw::move( tsk ) );
			return true;
		}
		for( size_t n = id + 1; n < std::size( m_impl->m_tasks ); ++n ) {
			if( auto tsk = m_impl->m_tasks[n].try_pop_front( ); tsk ) {
				run_task( daw::move( tsk ) );
				return true;
			}
		}
		for( size_t n = 0; n < id; ++n ) {
			if( auto tsk = m_impl->m_tasks[n].try_pop_front( ); tsk ) {
				run_task( daw::move( tsk ) );
				return true;
			}
		}
		return false;
	}

	void task_scheduler::add_queue( size_t n ) {
		assert( m_impl );
		auto const th_lck = std::lock_guard( m_impl->m_threads_mutex );
		auto &threads = m_impl->m_threads;
		try {
			threads.emplace_back(
			  []( daw::parallel::stop_token tok, size_t id, auto wself ) {
				  if( auto self = wself.lock( ); self ) {
					  self->task_runner( id );
				  }
			  },
			  n, get_handle( ) );
		} catch( std::system_error const &se ) {
			std::cerr << "Error creating thread, aborting.\n Error code: "
			          << se.code( ) << "\nMessage: " << se.what( ) << '\n';
			std::terminate( );
		}
	}

	void task_scheduler::start( ) {
		assert( m_impl );
		if( started( ) ) {
			return;
		}
		m_impl->m_continue = true;
		for( size_t n = 0; n < m_impl->m_num_threads; ++n ) {
			add_queue( n );
		}
	}

	void task_scheduler::task_scheduler_impl::stop( bool block_on_destruction ) {
		m_continue.store( false, std::memory_order_release );
		try {
			auto const th_lck = std::lock_guard( m_threads_mutex );
			for( auto &th : m_threads ) {
				try {
					if( block_on_destruction ) {
						th.stop_and_wait( );
					} else {
						th.detach( );
						th.stop( );
					}
				} catch( ... ) {}
			}
			m_threads.clear( );
		} catch( ... ) {}
	}

	void task_scheduler::stop( bool block ) {
		m_impl->stop( block );
	}

	daw::parallel::ithread task_scheduler::start_temp_task_runner( ) {
		assert( m_impl );
		return daw::parallel::ithread(
		  [id = m_impl->m_current_id++,
		   wself = get_handle( )]( daw::parallel::stop_token tok ) {
			  if( auto self = wself.lock( ); self ) {
				  self->task_runner( id, tok );
			  }
		  } );
	}

	bool task_scheduler::send_task( std::unique_ptr<daw::task_t> &&tsk,
	                                size_t id ) {
		if( not tsk ) {
			return true;
		}
		assert( m_impl );
		if( not m_impl->m_continue ) {
			return true;
		}
		assert( ( std::size( m_impl->m_tasks ) > id ) );
		if( m_impl->m_tasks[id].try_push_back( daw::move( tsk ) ) ==
		    daw::parallel::push_back_result::success ) {
			return true;
		}
		// Could not add to queue, try someone elses
		assert( m_impl->m_num_threads <= std::size( m_impl->m_tasks ) );
		for( auto m = ( id + 1 ) % m_impl->m_num_threads; m != id;
		     m = ( m + 1 ) % m_impl->m_num_threads ) {
			if( not m_impl->m_continue ) {
				return true;
			}
			if( m_impl->m_tasks[m].try_push_back( daw::move( tsk ) ) ==
			    daw::parallel::push_back_result::success ) {
				return true;
			}
		}
		// Could not add to another queue, wait for ours to have room
		return push_back( m_impl->m_tasks[id], daw::move( tsk ), [&]( ) {
			       return static_cast<bool>( m_impl->m_continue );
		       } ) == daw::parallel::push_back_result::success;
	}

	void task_scheduler::task_runner( size_t id ) {
		auto self = get_handle( ).lock( );
		if( not self ) {
			return;
		}
		assert( self->m_impl );
		bool keep_going =
		  self->m_impl->m_continue.load( std::memory_order_acquire );
		while( keep_going ) {
			auto tsk = self->wait_for_task_from_pool( id );
			keep_going = self->m_impl->m_continue.load( std::memory_order_acquire );
			if( not keep_going ) {
				return;
			}
			if( tsk ) {
				run_task( daw::move( tsk ) );
			} else if( id >= std::size( self->m_impl->m_tasks ) ) {
				return;
			}
		}
	}

	void task_scheduler::task_runner( size_t id, daw::parallel::stop_token tok ) {
		auto self = get_handle( ).lock( );
		if( not self ) {
			return;
		}
		assert( self->m_impl );

		bool keep_going =
		  self->m_impl->m_continue.load( std::memory_order_acquire );
		while( keep_going and tok ) {
			auto tsk = self->wait_for_task_from_pool( id, tok );
			keep_going = self->m_impl->m_continue.load( std::memory_order_acquire );
			if( keep_going and tok and tsk ) {
				run_task( daw::move( tsk ) );
			}
		}
	}

	void task_scheduler::task_runner( size_t id, daw::shared_latch &sem ) {
		auto self = get_handle( ).lock( );
		if( not self ) {
			return;
		}
		assert( self->m_impl );

		bool keep_going =
		  self->m_impl->m_continue.load( std::memory_order_acquire );
		while( keep_going and sem ) {
			auto tsk = self->wait_for_task_from_pool( id, sem );
			keep_going = self->m_impl->m_continue.load( std::memory_order_acquire );
			if( keep_going and sem and tsk ) {
				run_task( daw::move( tsk ) );
			}
		}
	}

	bool task_scheduler::has_empty_queue( ) const {
		assert( m_impl );
		for( auto &q : m_impl->m_tasks ) {
			if( q.is_empty( ) ) {
				return true;
			}
		}
		return false;
	}

	char const *unable_to_add_task_exception::what( ) const noexcept {
		return "Unable to add task";
	}

	std::shared_ptr<task_scheduler::task_scheduler_impl>
	task_scheduler::make_ts( std::size_t const num_threads,
	                         bool block_on_destruct ) {
		auto ptr =
		  std::make_shared<task_scheduler_impl>( num_threads, block_on_destruct );
		assert( std::size( ptr->m_tasks ) == num_threads );
		return ptr;
	}

	[[nodiscard]] bool task_scheduler::add_task( daw::shared_latch &&sem ) {
		return add_task( empty_task( ), daw::move( sem ) );
	}

	[[nodiscard]] bool task_scheduler::add_task( daw::shared_latch const &sem ) {
		return add_task( empty_task( ), sem );
	}
} // namespace daw
