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

#pragma once

#include <atomic>
#include <boost/optional.hpp>
#include <functional>
#include <list>
#include <memory>
#include <thread>
#include <vector>

#include <daw/daw_locked_stack.h>
#include <daw/daw_locked_value.h>
#include <daw/scope_guard.h>
#include <daw/daw_semaphore.h>
#include <daw/daw_utility.h>

#include "task_scheduler_impl.h"

namespace daw {
	class task_scheduler {
		std::shared_ptr<impl::task_scheduler_impl> m_impl;
	  public:
		task_scheduler( std::size_t num_threads = std::thread::hardware_concurrency( ),
		                bool block_on_destruction = true );
		~task_scheduler( ) = default;
		task_scheduler( task_scheduler && ) = default;
		task_scheduler &operator=( task_scheduler && ) = default;

		task_scheduler( task_scheduler const & ) = default;
		task_scheduler &operator=( task_scheduler const & ) = default;

		template<typename Task>
		void add_task( Task task ) noexcept {
			m_impl->add_task( std::move( task ) );
		}

		void start( );
		void stop( bool block = true ) noexcept;
		bool started( ) const;
		size_t size( ) const;

		template<typename Function>
		auto blocking_section( Function func ) {
			if( !m_impl->am_i_in_pool( ) ) {
				return func( );
			}
			auto semaphore = m_impl->start_temp_task_runners( );
			auto const at_exit = daw::on_scope_exit( [&semaphore]( ) { semaphore.notify( ); } );
			return func( );
		}

		template<typename Waitable>
		void blocking_on_waitable( Waitable waitable ) {
			blocking_section( [&waitable]( ) { waitable.wait( ); } );
		}
	}; // task_scheduler

	task_scheduler get_task_scheduler( );

	/// Add a single task to the supplied task scheduler and notify supplied semaphore when complete
	///
	/// @param semaphore Semaphore to notify when task is completed
	/// @param task Task of form void( ) to run
	/// @param ts task_scheduler to add task to
	template<typename Task>
	void schedule_task( daw::shared_semaphore semaphore, Task task, task_scheduler ts = get_task_scheduler( ) ) {
		ts.add_task( [ task = std::move( task ), semaphore = std::move( semaphore ) ]( ) mutable {
			task( );
			semaphore.notify( );
		} );
	}

	template<typename Task>
	daw::shared_semaphore create_waitable_task( Task task, task_scheduler ts = get_task_scheduler( ) ) {
		daw::shared_semaphore semaphore;
		schedule_task( semaphore, ts, std::move( task ) );
		return semaphore;
	}

	/// Run concurrent tasks and return when completed
	///
	/// @param tasks callable items of the form void( )
	/// @returns a semaphore that will stop waiting when all tasks complete
	template<typename... Tasks>
	daw::shared_semaphore create_task_group( Tasks &&... tasks ) {
		auto ts = get_task_scheduler( );
		daw::shared_semaphore semaphore{ 1 - sizeof...( tasks ) };

		auto const st = [&]( auto task ) {
			schedule_task( semaphore, std::move( task ), ts );
			return 0;
		};

		auto const dummy = {st( std::move( tasks ) )...};
		Unused( dummy );
		return semaphore;
	}

	/// Run concurrent tasks and return when completed
	///
	/// @param tasks callable items of the form void( )
	template<typename... Tasks>
	void invoke_tasks( Tasks... tasks ) {
		get_task_scheduler( ).blocking_on_waitable( create_task_group( tasks... ) );
	}

	template<typename Function>
	auto blocking_section( Function func, task_scheduler ts = get_task_scheduler( ) ) {
		return ts.blocking_section( func );
	}
} // namespace daw

