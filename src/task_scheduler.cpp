// The MIT License (MIT)
//
// Copyright (c) 2016 Darrell Wright
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
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include "task_scheduler.h"

namespace daw {
	task_scheduler::task_scheduler( std::size_t num_threads, bool block_on_destruction ):
			enable_shared_from_this<task_scheduler>{ },
			m_threads( ),
			m_tasks{ },
			m_continue{ true }, 
			m_block_on_destruction{ block_on_destruction },
			m_num_threads{ num_threads },
			m_task_count{ 0 } {
	
		for( size_t n=0; n<m_num_threads; ++n ) {
			m_tasks.emplace_back( );
		}
	}

	task_scheduler::~task_scheduler( ) { 
		stop( m_block_on_destruction );
	}

	void task_scheduler::run( ) {
		for( size_t n=0; n<m_num_threads; ++n ) {
			m_threads.emplace_back( [id=n, wself = get_weak_this( )]( ) {
					// The self.lock( ) determines where or not the 
					// task_scheduler has destructed yet and keeps it alive while
					// we use members
					while( true ) {
						task_t task = nullptr;
						{
							auto self = wself.lock( );
							if( !self || !self->m_continue ) {
							// Either we have destructed already or stop has been called
							break;
							}
							// Get task.  First try own queue, if not try the others and finally
							// wait for own queue to fill
							auto tsk = self->m_tasks[id].try_pop_back( );
							for( auto m = (id + 1)%self->m_num_threads; !tsk && m != id; m = (m+1)%self->m_num_threads ) {
								tsk = self->m_tasks[m].try_pop_back( );
							}
							if( tsk ) {
								task = *tsk;
							} else {
								task = self->m_tasks[id].pop_back( );
							}
						}
						if( task ) {	// Just in case we get an empty function
							try {
								task( );
							} catch(...) {
								// Don't let a task take down thread	
								// TODO: figure out what else we can do
							}
						}
					}
			} );
		}
	}

	void task_scheduler::stop( bool block ) noexcept {
		m_continue = false;
		for( auto & th: m_threads ) {
			try {
				if( th.joinable( ) ) {
					if( block ) {
						th.join( );
					} else {
						th.detach( );
					}
				}
			} catch( std::exception const & ) { }
		}
	}

	std::weak_ptr<task_scheduler> task_scheduler::get_weak_this( ) {
		std::weak_ptr<task_scheduler> result = shared_from_this( );
		return result;
	}

	void task_scheduler::add_task( task_scheduler::task_t task ) noexcept {
		if( task ) {	// Only allow valid tasks
			auto id = (m_task_count++)%m_num_threads;
			m_tasks[id].push_back( std::move( task ) );
		}
	}
}    // namespace daw

