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

#pragma once

#include <memory>
#include <thread>

#include <daw/daw_scope_guard.h>
#include <daw/daw_utility.h>
#include <daw/parallel/daw_latch.h>

#include "impl/task_scheduler_impl.h"

namespace daw {
	class task_scheduler {
		std::shared_ptr<impl::task_scheduler_impl> m_impl;

	public:
		explicit task_scheduler(
		  std::size_t num_threads = std::thread::hardware_concurrency( ),
		  bool block_on_destruction = true );

		template<typename Task, std::enable_if_t<traits::is_callable_v<Task>,
		                                         std::nullptr_t> = nullptr>
		void add_task( Task &&task ) noexcept {
			m_impl->add_task( std::forward<Task>( task ) );
		}

		template<typename Task, std::enable_if_t<traits::is_callable_v<Task>,
		                                         std::nullptr_t> = nullptr>
		void add_task( Task &&task, daw::shared_latch sem ) noexcept {
			m_impl->add_task( std::forward<Task>( task ), daw::move( sem ) );
		}

		void add_task( daw::shared_latch sem ) {
			m_impl->add_task( []( ) {}, daw::move( sem ) );
		}

		void start( );
		void stop( bool block = true ) noexcept;
		bool started( ) const;
		size_t size( ) const;

		template<typename Function>
		decltype( auto ) wait_for_scope( Function &&func ) {
			static_assert( traits::is_callable_v<Function>,
			               "Function passed to wait_for_scope must be callable "
			               "without an arugment. e.g. func( )" );

			auto const at_exit = daw::on_scope_exit(
			  [sem = m_impl->start_temp_task_runners( )]( ) mutable {
				  sem.notify( );
			  } );
			return func( );
		}

		template<typename Waitable>
		void wait_for( Waitable &&waitable ) {
			static_assert(
			  impl::is_waitable_v<Waitable>,
			  "Waitable must have a wait( ) member. e.g. waitable.wait( )" );

			wait_for_scope( [&waitable]( ) { waitable.wait( ); } );
		}

		explicit operator bool( ) const noexcept {
			return static_cast<bool>( m_impl ) && m_impl->started( );
		}
	}; // task_scheduler

	template<typename...>
	struct is_task_scheduler : public std::false_type {};

	template<>
	struct is_task_scheduler<task_scheduler> : public std::true_type {};

	template<typename...Args>
	inline constexpr bool is_task_scheduler_v = is_task_scheduler<Args...>::value;

	task_scheduler get_task_scheduler( );

	/// Add a single task to the supplied task scheduler and notify supplied
	/// semaphore when complete
	///
	/// @param sem Semaphore to notify when task is completed
	/// @param task Task of form void( ) to run
	/// @param ts task_scheduler to add task to
	template<typename Task>
	void schedule_task( daw::shared_latch sem, Task &&task,
	                    task_scheduler ts = get_task_scheduler( ) ) {
		static_assert( traits::is_callable_v<Task>,
		               "Task task passed to schedule_task must be callable without "
		               "an arugment. e.g. task( )" );
		ts.add_task(
		  [task = std::forward<Task>( task ), sem = daw::move( sem )]( ) mutable {
			  auto const at_exit = daw::on_scope_exit( [&]( ) { sem.notify( ); } );
			  task( );
		  } );
	}

	template<typename Task>
	daw::shared_latch
	create_waitable_task( Task &&task,
	                      task_scheduler ts = get_task_scheduler( ) ) {
		static_assert( traits::is_callable_v<Task>,
		               "Task task passed to create_waitable_task must be callable "
		               "without an arugment. "
		               "e.g. task( )" );
		auto sem = daw::shared_latch( );
		schedule_task( sem, std::forward<Task>( task ), ts );
		return sem;
	}

	/// Run concurrent tasks and return when completed
	///
	/// @param tasks callable items of the form void( )
	/// @returns a semaphore that will stop waiting when all tasks complete
	template<typename... Tasks>
	daw::shared_latch create_task_group( Tasks &&... tasks ) {
		static_assert( impl::are_tasks_v<Tasks...>,
		               "Tasks passed to create_task_group must be callable without "
		               "an arugment. e.g. task( )" );
		auto ts = get_task_scheduler( );
		auto sem = daw::shared_latch( sizeof...( tasks ) );

		auto const st = [&]( auto &&task ) {
			schedule_task( sem, std::forward<decltype( task )>( task ), ts );
			return 0;
		};

		Unused( (daw::invoke( st, std::forward<Tasks>( tasks ) ) +... ) );

		return sem;
	}

	/// Run concurrent tasks and return when completed
	///
	/// @param tasks callable items of the form void( )
	template<typename... Tasks>
	void invoke_tasks( task_scheduler ts, Tasks &&... tasks ) {
		ts.wait_for( create_task_group( std::forward<Tasks>( tasks )... ) );
	}

	template<typename... Tasks>
	void invoke_tasks( Tasks &&... tasks ) {
		static_assert( impl::are_tasks_v<Tasks...>,
		               "Tasks passed to invoke_tasks must be callable without an "
		               "arugment. e.g. task( )" );
		invoke_tasks( get_task_scheduler( ), std::forward<Tasks>( tasks )... );
	}

	template<typename Function>
	decltype( auto ) wait_for_scope( Function &&func,
	                                 task_scheduler ts = get_task_scheduler( ) ) {
		static_assert( traits::is_callable_v<Function>,
		               "Function passed to wait_for_scope must be callable without "
		               "an arugment. e.g. func( )" );
		return ts.wait_for_scope( std::forward<Function>( func ) );
	}
} // namespace daw
