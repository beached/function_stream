// The MIT License (MIT)
//
// Copyright (c) Darrell Wright
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

#include "daw/fs/task_scheduler.h"
#include "daw/fs/impl/daw_latch.h"
#include "daw/fs/impl/ithread.h"

#include <daw/daw_scope_guard.h>

#include <iostream>
#include <thread>

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
		requires( invocable<Callable, Args...> ) //
		  parallel::ithread create_thread( Callable &&callable, Args &&...args ) noexcept {
			try {
				return parallel::ithread( DAW_FWD( callable ), DAW_FWD( args )... );
			} catch( std::system_error const &e ) {
				std::cerr << "Error creating thread, aborting.\n Error code: " << e.code( )
				          << "\nMessage: " << e.what( ) << '\n';
				std::abort( );
			}
		}
	} // namespace

	fixed_task_scheduler::fixed_task_scheduler( std::size_t num_threads, bool block_on_destruction )
	  : m_num_threads( num_threads )
	  , m_tasks( )
	  , m_block_on_destruction( block_on_destruction ) {

		m_tasks.resize( m_num_threads );
		std::cout << m_tasks.size( ) << '\n';
	}

	fixed_task_scheduler::~fixed_task_scheduler( ) {
		stop( m_block_on_destruction );
	}

	void fixed_task_scheduler::stop( bool block_on_destruction ) {
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

	std::size_t fixed_task_scheduler::size( ) const {
		return m_tasks.size( );
	}

	bool fixed_task_scheduler::has_empty_queue( ) const {
		return daw::algorithm::contains_if( std::begin( m_tasks ),
		                                    std::end( m_tasks ),
		                                    []( auto const &q ) { return q.empty( ); } );
	}

	void fixed_task_scheduler::add_queue( std::size_t n, handle_t handle ) {
		auto threads = m_threads.get( );
		auto thread_map = m_thread_map.get( );
		auto thr = create_thread(
		  []( std::size_t id, handle_t wself ) {
			  auto self = wself.lock( );
			  if( not self ) {
				  return;
			  }
			  self->task_runner( id );
		  },
		  n,
		  DAW_MOVE( handle ) );
		auto id = thr.get_id( );
		threads->push_back( DAW_MOVE( thr ) );
		( *thread_map )[id] = n;
	}

	bool fixed_task_scheduler::started( ) const {
		return m_continue;
	}

	task_scheduler::task_scheduler( ) {
		start( );
	}

	task_scheduler::task_scheduler( std::size_t num_threads, bool block_on_destruction )
	  : m_ts_impl( new ts_t( num_threads, block_on_destruction ) ) {

		start( );
	}

	bool task_scheduler::started( ) const {
		assert( m_ts_impl );
		return m_ts_impl->started( );
	}

	void task_scheduler::add_queue( std::size_t n ) {
		assert( m_ts_impl );
		m_ts_impl->add_queue( n, get_handle( ) );
	}

	unique_task_t fixed_task_scheduler::wait_for_task_from_pool( std::size_t id ) {
		// Get task.  First try own queue, if not try the others and finally
		// wait for own queue to fill
		if( not started( ) ) {
			return { };
		}
		if( id < m_tasks.size( ) ) {
			if( auto tsk = m_tasks[id].try_pop_front( ); tsk ) {
				return DAW_MOVE( *tsk );
			}
		}
		for( std::size_t n = id + 1; n < std::size( m_tasks ); ++n ) {
			if( auto tsk = m_tasks[n].try_pop_front( ); tsk ) {
				return DAW_MOVE( *tsk );
			}
		}
		for( std::size_t n = 0; n < id; ++n ) {
			if( auto tsk = m_tasks[n].try_pop_front( ); tsk ) {
				return DAW_MOVE( *tsk );
			}
		}
#ifndef DEBUG
		auto result = pop_front( m_tasks[id], [&]( ) {
#else
		auto result = pop_front( m_tasks.at( id ), [&]( ) {
#endif
			return started( );
		} );
		if( not result ) {
			return unique_task_t( );
		}
		return DAW_MOVE( *result );
	}

	[[nodiscard]] unique_task_t fixed_task_scheduler::wait_for_task_from_pool( std::size_t id,
	                                                                           shared_cnt_sem sem ) {
		// Get task.  First try own queue, if not try the others and finally
		// wait for own queue to fill
		if( not started( ) ) {
			return { };
		}
		if( auto tsk = m_tasks[id].try_pop_front( ); tsk ) {
			if( not tsk ) {
				return unique_task_t( );
			}
			return DAW_MOVE( *tsk );
		}
		for( auto m = ( id + 1 ) % m_num_threads; started( ) and m != id;
		     m = ( m + 1 ) % m_num_threads ) {

			if( auto tsk = m_tasks[m].try_pop_front( ); tsk ) {
				return DAW_MOVE( *tsk );
			}
		}
		auto result = pop_front( m_tasks[id], [&]( ) {
			return static_cast<bool>( started( ) and not sem.try_wait( ) );
		} );
		if( not result ) {
			return unique_task_t( );
		}
		return DAW_MOVE( *result );
	}

	void fixed_task_scheduler::run_task( unique_task_t tsk ) noexcept {
		try {
			if( not started( ) ) {
				return;
			}
			if( tsk.try_wait( ) ) {
				(void)send_task( DAW_MOVE( tsk ), get_task_id( ) );
			} else {
				tsk.execute( );
			}
		} catch( ... ) {
			breakpoint( );
			// Don't let a task take down thread
			// TODO: add callback to task_scheduler for handling
			// task exceptions
		}
	}

	size_t fixed_task_scheduler::get_task_id( ) {
		auto const tc = m_task_count++;
		return tc % m_num_threads;
	}

	unique_task_t task_scheduler::wait_for_task_from_pool( std::size_t id ) {
		assert( m_ts_impl );
		return m_ts_impl->wait_for_task_from_pool( id );
	}

	unique_task_t task_scheduler::wait_for_task_from_pool( std::size_t id, shared_cnt_sem sem ) {
		assert( m_ts_impl );
		return m_ts_impl->wait_for_task_from_pool( id, DAW_MOVE( sem ) );
	}

	void task_scheduler::run_task( unique_task_t tsk ) noexcept {
		assert( m_ts_impl );
		m_ts_impl->run_task( DAW_MOVE( tsk ) );
	}

	size_t task_scheduler::get_task_id( ) {
		assert( m_ts_impl );
		return m_ts_impl->get_task_id( );
	}

	bool task_scheduler::run_next_task( std::size_t id ) {
		assert( m_ts_impl );
		if( auto tsk = m_ts_impl->m_tasks[id].try_pop_front( ); tsk ) {
			run_task( DAW_MOVE( *tsk ) );
			return true;
		}
		for( std::size_t n = id + 1; n < std::size( m_ts_impl->m_tasks ); ++n ) {
			if( auto tsk = m_ts_impl->m_tasks[n].try_pop_front( ); tsk ) {
				run_task( DAW_MOVE( *tsk ) );
				return true;
			}
		}
		for( std::size_t n = 0; n < id; ++n ) {
			if( auto tsk = m_ts_impl->m_tasks[n].try_pop_front( ); tsk ) {
				run_task( DAW_MOVE( *tsk ) );
				return true;
			}
		}
		return false;
	}

	void task_scheduler::start( ) {
		assert( m_ts_impl );
		if( started( ) ) {
			return;
		}
		m_ts_impl->m_continue = true;
		// assert( m_ts_impl->m_tasks.size( ) == m_ts_impl->m_num_threads );
		for( std::size_t n = 0; n < m_ts_impl->m_num_threads; ++n ) {
			add_queue( n );
		}
	}

	void task_scheduler::stop( bool block ) noexcept {
		assert( m_ts_impl );
		m_ts_impl->stop( block );
	}

	task_scheduler::temp_task_runner task_scheduler::start_temp_task_runner( ) {
		assert( m_ts_impl );
		auto sem = shared_cnt_sem( 2 );
		auto const ae = on_scope_exit( [&sem] { sem.notify( ); } );
		auto wself = get_handle( );
		using wself_t = decltype( wself );
		// *****
		struct tmp_worker {
			size_t m_id;
			wself_t m_wself;
			mutable shared_cnt_sem m_sem;

			inline void operator( )( ) const {
				auto self = m_wself.lock( );
				if( not self ) {
					return;
				}
				self->task_runner( m_id, m_sem );
			}
		};
		// *****
		size_t const id = m_ts_impl->m_current_id++;

		return { create_thread( tmp_worker{ id, get_handle( ), sem } ), sem };
	}

	bool task_scheduler::send_task( unique_task_t tsk, std::size_t id ) {
		assert( m_ts_impl );
		if( not m_ts_impl->m_continue ) {
			return true;
		}
		if( m_ts_impl->m_tasks[id].try_push_back( DAW_MOVE( tsk ) ) ==
		    parallel::push_back_result::success ) {
			return true;
		}
		for( auto m = ( id + 1 ) % m_ts_impl->m_num_threads; m != id;
		     m = ( m + 1 ) % m_ts_impl->m_num_threads ) {
			if( not m_ts_impl->m_continue ) {
				return true;
			}
			if( m_ts_impl->m_tasks[m].try_push_back( DAW_MOVE( tsk ) ) ==
			    parallel::push_back_result::success ) {
				return true;
			}
		}
		return push_back( m_ts_impl->m_tasks[id], DAW_MOVE( tsk ), [&]( ) {
			       return static_cast<bool>( m_ts_impl->m_continue );
		       } ) == parallel::push_back_result::success;
	}

	void task_scheduler::task_runner( std::size_t id ) {
		assert( m_ts_impl );
		auto w_self = get_handle( );
		while( true ) {
			auto tsk = unique_task_t( );
			{
				auto self = w_self.lock( );
				if( not self or not self->m_ts_impl->m_continue ) {
					return;
				}
				tsk = self->wait_for_task_from_pool( id );
			}
			run_task( DAW_MOVE( tsk ) );
		}
	}

	void task_scheduler::task_runner( std::size_t id, shared_cnt_sem &sem ) {
		assert( m_ts_impl );
		auto w_self = get_handle( );
		while( not sem.try_wait( ) ) {
			auto tsk = unique_task_t( );
			{
				auto self = w_self.lock( );
				if( not self or not self->m_ts_impl->m_continue or not sem.try_wait( ) ) {
					return;
				}
				tsk = self->wait_for_task_from_pool( id, sem );
			}
			run_task( DAW_MOVE( tsk ) );
		}
	}

	char const *unable_to_add_task_exception::what( ) const noexcept {
		return "Unable to add task";
	}
} // namespace daw
