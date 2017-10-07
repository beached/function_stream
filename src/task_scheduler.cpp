// The MIT License (MIT)
//
// Copyright (c) 2016-2017 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <condition_variable>
#include <functional>
#include <iostream>
#include <thread>

#include <daw/daw_semaphore.h>

#include "task_scheduler.h"

namespace daw {
	namespace impl {
		namespace {
			template<typename... Args>
			auto create_thread( Args &&... args ) noexcept {
				try {
					return std::thread{std::forward<Args>( args )...};
				} catch( std::system_error const &e ) {
					std::cerr << "Error creating thread, aborting.\n Error code: " << e.code( ) << "\nMessage: " << e.what( )
					          << std::endl;
					std::terminate( );
				}
			}
		} // namespace
		task_scheduler_impl::task_scheduler_impl( std::size_t num_threads, bool block_on_destruction )
		  : m_continue{false}
		  , m_block_on_destruction{block_on_destruction}
		  , m_num_threads{num_threads}
		  , m_tasks{}
		  , m_task_count{0}
		  , m_other_threads{} {

			for( size_t n = 0; n < m_num_threads; ++n ) {
				// TODO: right size the task queue
				m_tasks.emplace_back( 1024 );
			}
		}

		task_scheduler_impl::~task_scheduler_impl( ) {
			stop( m_block_on_destruction );
		}

		bool task_scheduler_impl::started( ) const {
			return m_continue;
		}

		void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself,
		                  boost::optional<daw::shared_semaphore> sem ) {
			// The self.lock( ) determines where or not the
			// task_scheduler_impl has destructed yet and keeps it alive while
			// we use members
			while( !sem || !sem->try_wait( ) ) {
				auto self = wself.lock( );
				if( !self || !self->m_continue ) {
					// Either we have destructed already or stop has been called
					break;
				}
				// Get task.  First try own queue, if not try the others and finally
				// wait for own queue to fill
				daw::impl::task_ptr_t tsk;
				auto is_popped = self->m_tasks[id].receive( tsk );
				for( auto m = ( id + 1 ) % self->m_num_threads; !is_popped && m != id; m = ( m + 1 ) % self->m_num_threads ) {
					is_popped = self->m_tasks[m].receive( tsk );
				}
				if( !is_popped ) {
					while( !self->m_tasks[id].receive( tsk ) ) {
						using namespace std::chrono_literals;
						std::this_thread::sleep_for( 1ns );
					}
				}
				task_t task{tsk.move_out( )};
				try {
					task( );
				} catch( ... ) {
					// Don't let a task take down thread
					// TODO: add callback to task_scheduler for handling
					// task exceptions
				}
			}
		}

		void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself ) {
			task_runner( id, std::move( wself ), boost::none );
		}

		void task_scheduler_impl::start( ) {
			m_continue = true;
			auto threads = m_threads.get( );
			for( size_t n = 0; n < m_num_threads; ++n ) {
				threads->push_back(
				  create_thread( []( size_t id, auto wself ) { impl::task_runner( id, wself ); }, n, get_weak_this( ) ) );
			}
		}

		void task_scheduler_impl::stop( bool block ) noexcept {
			m_continue = false;
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
		}

		std::weak_ptr<task_scheduler_impl> task_scheduler_impl::get_weak_this( ) {
			std::shared_ptr<task_scheduler_impl> sp = this->shared_from_this( );
			std::weak_ptr<task_scheduler_impl> wp = sp;
			return wp;
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

		daw::shared_semaphore task_scheduler_impl::start_temp_task_runners( size_t task_count ) {
			daw::shared_semaphore sem{1 - task_count};
			for( size_t n = 0; n < task_count; ++n ) {
				auto const id = [&]( ) {
					auto const current_epoch =
					  static_cast<size_t>( ( std::chrono::high_resolution_clock::now( ) ).time_since_epoch( ).count( ) );
					return current_epoch % size( );
				}( );

				auto other_threads = m_other_threads.get( );
				auto it = other_threads->emplace( other_threads->end( ), boost::none );
				other_threads.release( );

				auto const thread_worker = [ it, id, wself = get_weak_this( ), sem ]( ) mutable {
					auto const at_exit = daw::on_scope_exit( [&wself, it]( ) {
						auto self = wself.lock( );
						if( self ) {
							self->m_other_threads.get( )->erase( it );
						}
					} );
					task_runner( id, wself, sem );
				};
				*it = create_thread( thread_worker );
				( *it )->detach( );
			}
			return sem;
		}
	} // namespace impl

	task_scheduler::task_scheduler( std::size_t num_threads, bool block_on_destruction )
	  : m_impl{std::make_shared<impl::task_scheduler_impl>( num_threads, block_on_destruction )} {}

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
		return ts;
	}
} // namespace daw
