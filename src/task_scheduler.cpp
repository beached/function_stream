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

	void fixed_task_scheduler::add_queue( std::size_t n, ts_handle_t handle ) {
		auto threads = m_threads.get( );
		auto thread_map = m_thread_map.get( );
		auto thr = create_thread(
		  []( std::size_t id, ts_handle_t wself ) {
			  auto self = wself.lock( );
			  if( not self ) {
				  return;
			  }
			  self->task_runner( id );
		  },
		  n,
		  std::move( handle ) );
		auto id = thr.get_id( );
		threads->push_back( std::move( thr ) );
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

	std::optional<unique_task_t> fixed_task_scheduler::try_pop_any( std::size_t id ) {
		// Try own queue first, then steal from the others
		auto const sz = std::size( m_tasks );
		for( std::size_t n = 0; n < sz; ++n ) {
			if( auto tsk = m_tasks[( id + n ) % sz].try_pop_front( ); tsk ) {
				return tsk;
			}
		}
		return { };
	}

	unique_task_t fixed_task_scheduler::wait_for_task_from_pool( std::size_t id ) {
		// Keep polling every queue, not just our own.  Otherwise a task sent to the
		// queue of a worker that is blocked inside another task is never run
		while( started( ) ) {
			if( auto tsk = try_pop_any( id ); tsk ) {
				return std::move( *tsk );
			}
			std::this_thread::yield( );
		}
		return unique_task_t( );
	}

	[[nodiscard]] unique_task_t fixed_task_scheduler::wait_for_task_from_pool( std::size_t id,
	                                                                           shared_cnt_sem sem ) {
		while( started( ) and not sem.try_wait( ) ) {
			if( auto tsk = try_pop_any( id ); tsk ) {
				return std::move( *tsk );
			}
			std::this_thread::yield( );
		}
		return unique_task_t( );
	}

	void fixed_task_scheduler::run_task( unique_task_t tsk ) noexcept {
		try {
			if( not started( ) ) {
				return;
			}
			if( tsk.try_wait( ) ) {
				(void)send_task( std::move( tsk ), get_task_id( ) );
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
		return m_ts_impl->wait_for_task_from_pool( id, std::move( sem ) );
	}

	void task_scheduler::run_task( unique_task_t tsk ) noexcept {
		assert( m_ts_impl );
		m_ts_impl->run_task( std::move( tsk ) );
	}

	size_t task_scheduler::get_task_id( ) {
		assert( m_ts_impl );
		return m_ts_impl->get_task_id( );
	}

	bool fixed_task_scheduler::run_next_task( std::size_t id ) {
		if( auto tsk = m_tasks[id].try_pop_front( ); tsk ) {
			run_task( std::move( *tsk ) );
			return true;
		}
		for( std::size_t n = id + 1; n < std::size( m_tasks ); ++n ) {
			if( auto tsk = m_tasks[n].try_pop_front( ); tsk ) {
				run_task( std::move( *tsk ) );
				return true;
			}
		}
		for( std::size_t n = 0; n < id; ++n ) {
			if( auto tsk = m_tasks[n].try_pop_front( ); tsk ) {
				run_task( std::move( *tsk ) );
				return true;
			}
		}
		return false;
	}

	bool task_scheduler::run_next_task( std::size_t id ) {
		assert( m_ts_impl );
		return m_ts_impl->run_next_task( id );
	}

	void fixed_task_scheduler::start( ts_handle_t hnd ) {
		if( started( ) ) {
			return;
		}
		m_continue = true;
		// assert( m_ts_impl->m_tasks.size( ) == m_ts_impl->m_num_threads );
		for( std::size_t n = 0; n < m_num_threads; ++n ) {
			add_queue( n, hnd );
		}
	}

	void task_scheduler::start( ) {
		assert( m_ts_impl );
		m_ts_impl->start( get_handle( ) );
	}

	void task_scheduler::stop( bool block ) noexcept {
		assert( m_ts_impl );
		m_ts_impl->stop( block );
	}

	fixed_task_scheduler::temp_task_runner
	fixed_task_scheduler::start_temp_task_runner( ts_handle_t wself ) {
		auto sem = shared_cnt_sem( 2 );
		auto const ae = on_scope_exit( [&sem] { sem.notify( ); } );
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
		size_t const id = m_current_id++;

		return { create_thread( tmp_worker{ id, wself, sem } ), sem };
	}

	fixed_task_scheduler::temp_task_runner task_scheduler::start_temp_task_runner( ) {
		assert( m_ts_impl );
		return m_ts_impl->start_temp_task_runner( get_handle( ) );
	}

	bool fixed_task_scheduler::send_task( unique_task_t tsk, std::size_t id ) {
		if( not started( ) ) {
			return true;
		}
		if( m_tasks[id].try_push_back( std::move( tsk ) ) == parallel::push_back_result::success ) {
			return true;
		}
		for( auto m = ( id + 1 ) % m_num_threads; m != id; m = ( m + 1 ) % m_num_threads ) {
			if( not started( ) ) {
				return true;
			}
			if( m_tasks[m].try_push_back( std::move( tsk ) ) == parallel::push_back_result::success ) {
				return true;
			}
		}
		auto const res = push_back( m_tasks[id], std::move( tsk ), [&]( ) { return started( ); } );
		return res == parallel::push_back_result::success;
	}

	bool task_scheduler::send_task( unique_task_t tsk, std::size_t id ) {
		assert( m_ts_impl );
		return m_ts_impl->send_task( std::move( tsk ), id );
	}

	void task_scheduler::task_runner( std::size_t id ) {
		assert( m_ts_impl );
		auto w_self = get_handle( );
		while( true ) {
			auto tsk = unique_task_t( );
			{
				auto self = w_self.lock( );
				if( not self or not self->started( ) ) {
					return;
				}
				tsk = self->wait_for_task_from_pool( id );
			}
			run_task( std::move( tsk ) );
		}
	}

	void task_scheduler::task_runner( std::size_t id, shared_cnt_sem &sem ) {
		assert( m_ts_impl );
		auto w_self = get_handle( );
		while( not sem.try_wait( ) ) {
			auto tsk = unique_task_t( );
			{
				auto self = w_self.lock( );
				if( not self or not self->started( ) or not sem.try_wait( ) ) {
					return;
				}
				tsk = self->wait_for_task_from_pool( id, sem );
			}
			run_task( std::move( tsk ) );
		}
	}

	char const *unable_to_add_task_exception::what( ) const noexcept {
		return "Unable to add task";
	}

	unable_to_add_task_exception::~unable_to_add_task_exception( ) = default;

	ts_handle_t::ts_handle_t( std::shared_ptr<fixed_task_scheduler> &ts )
	  : m_handle( ts ) {}

	std::optional<task_scheduler> ts_handle_t::lock( ) const {
		if( auto lck = m_handle.lock( ); lck ) {
			return std::optional<task_scheduler>( std::in_place, std::move( lck ) );
		}
		return { };
	}

	bool ts_handle_t::expired( ) const {
		return m_handle.expired( );
	}
} // namespace daw
