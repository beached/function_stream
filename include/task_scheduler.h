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
#include <daw/daw_semaphore.h>
#include <daw/daw_utility.h>

namespace daw {
	struct task_scheduler_impl;

	namespace impl {
		void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself,
		                  boost::optional<daw::shared_semaphore> semaphore = boost::none );
	}

	void blocking( std::function<void( )> task, size_t task_count = 1 );

	struct task_scheduler_impl : public std::enable_shared_from_this<task_scheduler_impl> {
		using task_t = std::function<void( )>;

	  private:
		using task_queue_t = daw::locked_stack_t<task_t>;
		std::vector<std::thread> m_threads;
		std::vector<task_queue_t> m_tasks;
		std::atomic_bool m_continue;
		std::weak_ptr<task_scheduler_impl> get_weak_this( );
		bool m_block_on_destruction;
		size_t m_num_threads;
		std::atomic_uintmax_t m_task_count;
		daw::lockable_value_t<std::list<boost::optional<std::thread>>> m_other_threads;
		friend void impl::task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself,
		                               boost::optional<daw::shared_semaphore> semaphore );

		friend void blocking( std::function<void( )> task, size_t task_count );

	  public:
		task_scheduler_impl( std::size_t num_threads, bool block_on_destruction );
		~task_scheduler_impl( );
		task_scheduler_impl( task_scheduler_impl && ) = delete;            // TODO: investigate why implicitly deleted
		task_scheduler_impl &operator=( task_scheduler_impl && ) = delete; // TODO: investigate why implicitly deleted

		task_scheduler_impl( task_scheduler_impl const & ) = delete;
		task_scheduler_impl &operator=( task_scheduler_impl const & ) = delete;

		void add_task( task_t task ) noexcept;
		void start( );
		void stop( bool block = true ) noexcept;
		bool started( ) const;
		size_t size( ) const {
			return m_tasks.size( );
		}
		bool am_i_in_pool( ) const noexcept;
	}; // task_scheduler_impl

	class task_scheduler {
		std::shared_ptr<task_scheduler_impl> m_impl;

		friend void blocking( std::function<void( )> task, size_t task_count );

	  public:
		task_scheduler( std::size_t num_threads = std::thread::hardware_concurrency( ),
		                bool block_on_destruction = true );
		~task_scheduler( ) = default;
		task_scheduler( task_scheduler && ) = default;
		task_scheduler &operator=( task_scheduler && ) = default;

		task_scheduler( task_scheduler const & ) = default;
		task_scheduler &operator=( task_scheduler const & ) = default;

		void add_task( task_scheduler_impl::task_t task ) noexcept;
		void start( );
		void stop( bool block = true ) noexcept;
		bool started( ) const;
		size_t size( ) const {
			return m_impl->size( );
		}

		template<typename Function>
		void blocking_section( Function func ) {
			if( m_impl->am_i_in_pool( ) ) {
				blocking( func, 1 );
			} else {
				func( );
			}
		}
	}; // task_scheduler

	task_scheduler get_task_scheduler( );

	/// Add a single task to the supplied task scheduler and notify supplied semaphore when complete
	///
	/// @param semaphore Semaphore to notify when task is completed
	/// @param task Task of form void( ) to run
	/// @param ts task_scheduler to add task to
	template<typename Task>
	void schedule_task( daw::shared_semaphore semaphore, Task task, task_scheduler &ts ) {
		ts.add_task( [ task = std::move( task ), semaphore = std::move( semaphore ) ]( ) mutable {
			task( );
			semaphore.notify( );
		} );
	}

	template<typename Task>
	void schedule_task( daw::shared_semaphore semaphore, Task task ) {
		auto ts = get_task_scheduler( );
		schedule_task( std::move( semaphore ), std::move( task ), ts );
	}

	template<typename Task>
	daw::shared_semaphore create_waitable_task( Task task, task_scheduler & ts ) {
		daw::shared_semaphore semaphore;
		schedule_task( semaphore, ts, task );
		return semaphore;
	}

	template<typename Task>
	daw::shared_semaphore create_waitable_task( Task task ) { 
		auto ts = get_task_scheduler( );
		return create_waitable_task( std::move( task ), ts );
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

		// auto const dummy = { ( impl::schedule_task( semaphore, ts, tasks ), 0 )... };
		auto const dummy = {st( tasks )...};
		Unused( dummy );
		return semaphore;
	}

	/// Run concurrent tasks and return when completed
	///
	/// @param tasks callable items of the form void( )
	template<typename... Tasks>
	void invoke_tasks( Tasks &&... tasks ) {
		blocking_section( [&]( ) { create_task_group( std::forward<Tasks>( tasks )... ).wait( ); } );
	}

	template<typename Function>
	void blocking_section( task_scheduler & ts, Function && func ) {
		ts.blocking_section( std::forward<Function>( func ) );
	}

	template<typename Function>
	void blocking_section( Function && func ) {
		auto ts = get_task_scheduler( );
		ts.blocking_section( std::forward<Function>( func ) );
	}
} // namespace daw

