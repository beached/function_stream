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

#include <boost/next_prior.hpp>
#include <daw/parallel/daw_latch.h>

#include "daw/fs/task_scheduler.h"

namespace daw {
	namespace impl {
		namespace {
			template<typename... Args>
			auto create_thread( Args &&... args ) noexcept {
				try {
					return std::thread( std::forward<Args>( args )... );
				} catch( std::system_error const &e ) {
					std::cerr << "Error creating thread, aborting.\n Error code: "
					          << e.code( ) << "\nMessage: " << e.what( ) << std::endl;
					std::terminate( );
				}
			}
		} // namespace

		task_scheduler_impl::task_scheduler_impl( std::size_t num_threads,
		                                          bool block_on_destruction )
		  : m_block_on_destruction( block_on_destruction )
		  , m_num_threads( num_threads )
		  , m_tasks( make_task_queues( num_threads, 1024 ) ) {}

		task_scheduler_impl::~task_scheduler_impl( ) {
			stop( m_block_on_destruction );
		}

		bool task_scheduler_impl::started( ) const {
			return m_continue;
		}

		std::optional<daw::task_t>
		task_scheduler_impl::wait_for_task_from_pool( size_t id ) {
			// Get task.  First try own queue, if not try the others and finally
			// wait for own queue to fill
			if( !m_continue ) {
				return {};
			}
			if( auto tsk = m_tasks[id].try_pop_front( ); tsk ) {
				return tsk;
			}
			for( auto m = ( id + 1 ) % m_num_threads; m_continue and m != id;
			     m = ( m + 1 ) % m_num_threads ) {

				if( auto tsk = m_tasks[m].try_pop_front( ); tsk ) {
					return tsk;
				}
			}
			return m_tasks[id].pop_front( m_continue );
		}

		void
		task_scheduler_impl::run_task( std::optional<daw::task_t> &&tsk ) noexcept {
			if( !tsk or !tsk->m_function ) {
				return;
			}
			try {
				if( tsk->is_ready( ) ) {
					send_task( std::move( tsk ), get_task_id( ) );
				} else {
					std::invoke( *tsk );
				}
			} catch( std::exception const &ex ) {
				// Don't let a task take down thread
				// TODO: add callback to task_scheduler for handling
				// task exceptions
				Unused( ex );
			} catch( ... ) { Unused( tsk ); }
		}

		size_t task_scheduler_impl::get_task_id( ) {
			auto const tc = m_task_count++;
			return tc % m_num_threads;
		}

		bool task_scheduler_impl::run_next_task( size_t id ) {
			if( auto tsk = m_tasks[id].try_pop_front( ); tsk ) {
				run_task( daw::move( tsk ) );
				return true;
			}
			for( auto m = ( id + 1 ) % m_num_threads; m != id;
			     m = ( m + 1 ) % m_num_threads ) {

				if( auto tsk = m_tasks[m].try_pop_front( ); tsk ) {
					run_task( daw::move( tsk ) );
					return true;
				}
			}
			return false;
		}

		void
		task_scheduler_impl::task_runner( size_t id,
		                                  std::weak_ptr<task_scheduler_impl> wself,
		                                  std::optional<daw::shared_latch> sem ) {

			// The self.lock( ) determines where or not the
			// task_scheduler_impl has destructed yet and keeps it alive while
			// we use members
			while( !sem or !sem->try_wait( ) ) {
				auto self = wself.lock( );
				if( !self or !self->m_continue ) {
					return;
				}
				run_task( self->wait_for_task_from_pool( id ) );
			}
		}

		void task_scheduler_impl::task_runner(
		  size_t id, std::weak_ptr<task_scheduler_impl> wself ) {

			auto const self = wself.lock( );
			if( !self ) {
				return;
			}
			while( self->m_continue ) {
				run_task( self->wait_for_task_from_pool( id ) );
			}
		}

		void task_scheduler_impl::start( ) {
			m_continue = true;
			auto threads = m_threads.get( );
			auto thread_map = m_thread_map.get( );
			for( size_t n = 0; n < m_num_threads; ++n ) {
				auto thr = create_thread(
				  []( size_t id, auto wself ) {
					  auto self = wself.lock( );
					  if( !self ) {
						  return;
					  }
					  self->task_runner( id, wself );
				  },
				  n, get_weak_this( ) );
				auto id = thr.get_id( );
				threads->push_back( daw::move( thr ) );
				( *thread_map )[id] = n;
			}
		}

		void task_scheduler_impl::stop( bool block ) noexcept {
			m_continue = false;
			try {
				for( size_t n = 0; n < m_tasks.size( ); ++n ) {
					add_task( []( ) {}, n );
				}
				auto threads = m_threads.get( );
				for( auto &th : *threads ) {
					try {
						if( th.joinable( ) ) {
							if( block ) {
								th.join( );
							} else {
								th.detach( );
							}
						}
					} catch( std::exception const & ) {}
				}
				threads->clear( );
			} catch( ... ) {}
		}

		std::weak_ptr<task_scheduler_impl> task_scheduler_impl::get_weak_this( ) {
			return static_cast<std::weak_ptr<task_scheduler_impl>>(
			  this->shared_from_this( ) );
		}

		bool task_scheduler_impl::am_i_in_pool( ) const noexcept {
			auto const id = std::this_thread::get_id( );
			{
				auto threads = m_threads.get( );
				for( auto const &th : *threads ) {
					if( th.get_id( ) == id ) {
						return true;
					}
				}
			}
			auto other_threads = m_other_threads.get( );
			for( auto const &th : *other_threads ) {
				if( th && th->get_id( ) == id ) {
					return true;
				}
			}
			return false;
		}

		daw::shared_latch
		task_scheduler_impl::start_temp_task_runners( size_t task_count ) {
			auto sem = daw::shared_latch( task_count );
			for( size_t n = 0; n < task_count; ++n ) {
				auto const id = [&]( ) {
					auto const current_epoch =
					  static_cast<size_t>( ( std::chrono::high_resolution_clock::now( ) )
					                         .time_since_epoch( )
					                         .count( ) );
					return current_epoch % size( );
				}( );

				auto other_threads = m_other_threads.get( );
				auto it = other_threads->emplace( other_threads->end( ), std::nullopt );
				other_threads.release( );

				auto const thread_worker = [it, id, wself = get_weak_this( ),
				                            sem]( ) mutable {
					auto self = wself.lock( );
					if( !self ) {
						return;
					}
					auto const at_exit = daw::on_scope_exit(
					  [&self, it]( ) { self->m_other_threads.get( )->erase( it ); } );

					self->task_runner( id, wself, sem );
				};
				*it = create_thread( thread_worker );
				( *it )->detach( );
			}
			return sem;
		}

		void task_scheduler_impl::send_task( std::optional<task_t> &&tsk,
		                                     size_t id ) {
			if( !tsk ) {
				return;
			}
			if( !m_continue ) {
				return;
			}
			if( m_tasks[id].try_push_back( std::move( *tsk ) ) ==
			    daw::parallel::push_back_result::success ) {
				return;
			}
			for( auto m = ( id + 1 ) % m_num_threads; m != id;
			     m = ( m + 1 ) % m_num_threads ) {
				if( !m_continue ) {
					return;
				}
				if( m_tasks[m].try_push_back( std::move( *tsk ) ) ==
				    daw::parallel::push_back_result::success ) {
					return;
				}
			}
			m_tasks[id].push_back( std::move( *tsk ), m_continue );
		}
	} // namespace impl

	task_scheduler::task_scheduler( std::size_t num_threads,
	                                bool block_on_destruction )
	  : m_impl{std::make_shared<impl::task_scheduler_impl>(
	      num_threads, block_on_destruction )} {}

	void task_scheduler::start( ) {
		m_impl->start( );
	}

	void task_scheduler::stop( bool block ) noexcept {
		m_impl->stop( block );
	}

	bool task_scheduler::started( ) const {
		return m_impl->started( );
	}

	size_t task_scheduler::size( ) const {
		return m_impl->size( );
	}

	task_scheduler get_task_scheduler( ) {
		static auto ts = []( ) {
			task_scheduler result;
			result.start( );
			return result;
		}( );
		if( !ts ) {
			ts.start( );
		}
		return ts;
	}
} // namespace daw
