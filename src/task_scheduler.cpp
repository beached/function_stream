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
		std::thread create_thread( Args &&... args ) noexcept {
			try {
				return std::thread( std::forward<Args>( args )... );
			} catch( std::system_error const &e ) {
				std::cerr << "Error creating thread, aborting.\n Error code: "
				          << e.code( ) << "\nMessage: " << e.what( ) << std::endl;
				std::abort( );
			}
		}
	} // namespace

	task_scheduler::member_data_t::member_data_t( std::size_t num_threads,
	                                              bool block_on_destruction )
	  : m_block_on_destruction( block_on_destruction )
	  , m_num_threads( num_threads )
	  , m_tasks( make_task_queues( num_threads ) ) {}

	task_scheduler::task_scheduler( std::size_t num_threads,
	                                bool block_on_destruction )
	  : m_data( new member_data_t( num_threads, block_on_destruction ) ) {}

	task_scheduler::~task_scheduler( ) {
		if( auto tmp = std::exchange( m_data, nullptr ); tmp ) {
			stop( tmp->m_block_on_destruction );
		}
	}

	bool task_scheduler::started( ) const {
		return m_data->m_continue;
	}

	std::optional<daw::task_t>
	task_scheduler::wait_for_task_from_pool( size_t id ) {
		// Get task.  First try own queue, if not try the others and finally
		// wait for own queue to fill
		if( not m_data->m_continue ) {
			return {};
		}
		if( auto tsk = m_data->m_tasks[id].try_pop_front( ); tsk ) {
			return tsk;
		}
		for( auto m = ( id + 1 ) % m_data->m_num_threads;
		     m_data->m_continue and m != id;
		     m = ( m + 1 ) % m_data->m_num_threads ) {

			if( auto tsk = m_data->m_tasks[m].try_pop_front( ); tsk ) {
				return tsk;
			}
		}
		return m_data->m_tasks[id].pop_front(
		  [m_continue = &( m_data->m_continue )]( ) {
			  return static_cast<bool>( *m_continue );
		  } );
	}

	void task_scheduler::run_task( std::optional<daw::task_t> &&tsk ) noexcept {
		if( not tsk ) {
			return;
		}
		try {
			if( m_data->m_continue ) {
				if( tsk->is_ready( ) ) {
					(void)send_task( daw::move( tsk ), get_task_id( ) );
				} else {
					(void)std::invoke( *tsk );
				}
			}
		} catch( ... ) {
			// Don't let a task take down thread
			// TODO: add callback to task_scheduler for handling
			// task exceptions
		}
	}

	size_t task_scheduler::get_task_id( ) {
		auto const tc = m_data->m_task_count++;
		return tc % m_data->m_num_threads;
	}

	bool task_scheduler::run_next_task( size_t id ) {
		if( auto tsk = m_data->m_tasks[id].try_pop_front( ); tsk ) {
			run_task( daw::move( tsk ) );
			return true;
		}
		for( auto m = ( id + 1 ) % m_data->m_num_threads; m != id;
		     m = ( m + 1 ) % m_data->m_num_threads ) {

			if( auto tsk = m_data->m_tasks[m].try_pop_front( ); tsk ) {
				run_task( daw::move( tsk ) );
				return true;
			}
		}
		return false;
	}

	void task_scheduler::start( ) {
		m_data->m_continue = true;
		auto threads = m_data->m_threads.get( );
		auto thread_map = m_data->m_thread_map.get( );
		for( size_t n = 0; n < m_data->m_num_threads; ++n ) {
			auto thr = create_thread(
			  []( size_t id, auto wself ) {
				  auto self = wself.lock( );
				  if( !self ) {
					  return;
				  }
				  self->task_runner( id, wself );
			  },
			  n, get_handle( ) );
			auto id = thr.get_id( );
			threads->push_back( daw::move( thr ) );
			( *thread_map )[id] = n;
		}
	}

	void task_scheduler::stop( bool block ) noexcept {
		if( auto tmp = m_data; tmp ) {
			tmp->m_continue = false;
			try {
				auto threads = tmp->m_threads.get( );
				for( auto &th : *threads ) {
					try {
						if( th.joinable( ) ) {
							if( block ) {
								th.join( );
							} else {
								th.detach( );
							}
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
		for( size_t n = 0; n < task_count; ++n ) {
			auto const id = [&]( ) {
				auto const current_epoch =
				  static_cast<size_t>( ( std::chrono::high_resolution_clock::now( ) )
				                         .time_since_epoch( )
				                         .count( ) );
				return current_epoch % size( );
			}( );

			auto other_threads = m_data->m_other_threads.get( );
			auto it = other_threads->emplace( other_threads->end( ), std::nullopt );
			other_threads.release( );

			*it = create_thread( [it, id, wself = get_handle( ), sem]( ) mutable {
				auto self = wself.lock( );
				if( !self ) {
					return;
				}
				auto const at_exit = daw::on_scope_exit( [&self, it]( ) {
					self->m_data->m_other_threads.get( )->erase( it );
				} );

				self->task_runner( id, wself, sem );
			} );
			( *it )->detach( );
		}
		return sem;
	}

	bool task_scheduler::send_task( std::optional<task_t> &&tsk, size_t id ) {
		if( !tsk ) {
			return true;
		}
		if( not m_data->m_continue ) {
			return true;
		}
		if( m_data->m_tasks[id].try_push_back( std::move( *tsk ) ) ==
		    daw::parallel::push_back_result::success ) {
			return true;
		}
		for( auto m = ( id + 1 ) % m_data->m_num_threads; m != id;
		     m = ( m + 1 ) % m_data->m_num_threads ) {
			if( not m_data->m_continue ) {
				return true;
			}
			if( m_data->m_tasks[m].try_push_back( std::move( *tsk ) ) ==
			    daw::parallel::push_back_result::success ) {
				return true;
			}
		}
		return m_data->m_tasks[id].push_back(
		  std::move( *tsk ), [m_continue = &( m_data->m_continue )]( ) {
			  return static_cast<bool>( *m_continue );
		  } );
	}

	char const *unable_to_add_task_exception::what( ) const noexcept {
		return "Unable to add task";
	}
} // namespace daw
