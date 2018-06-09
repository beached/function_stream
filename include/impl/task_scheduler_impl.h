// The MIT License (MIT)
//
// Copyright (c) 2016-2018 Darrell Wright
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

#include <atomic>
#include <boost/optional.hpp>
#include <functional>
#include <list>
#include <memory>
#include <thread>
#include <vector>

#include <daw/cpp_17.h>
#include <daw/daw_latch.h>
#include <daw/daw_locked_stack.h>
#include <daw/daw_locked_value.h>
#include <daw/daw_utility.h>

#include "message_queue.h"

namespace daw {
	struct task_t {
		std::function<void( )> m_function;
		std::unique_ptr<daw::shared_latch> m_semaphore;

		template<typename Callable, std::enable_if_t<daw::is_callable_v<Callable>,
		                                             std::nullptr_t> = nullptr>
		task_t( Callable &&c )
		  : m_function( std::forward<Callable>( c ) )
		  , m_semaphore( nullptr ) {

			daw::exception::Assert( static_cast<bool>( m_function ),
			                        "Callable must be valid" );
		}

		template<typename Callable, std::enable_if_t<daw::is_callable_v<Callable>,
		                                             std::nullptr_t> = nullptr>
		task_t( Callable &&c, daw::shared_latch sem )
		  : m_function( std::forward<Callable>( c ) )
		  , m_semaphore( std::make_unique<daw::shared_latch>(
		      std::move( sem ) ) ) {

			daw::exception::Assert( static_cast<bool>( m_function ),
			                        "Callable must be valid" );
		}

		void operator( )( ) noexcept( noexcept( m_function( ) ) ) {
			m_function( );
		}

		void operator( )( ) const noexcept( noexcept( m_function( ) ) ) {
			m_function( );
		}

		bool is_ready( ) const {
			if( m_semaphore ) {
				return m_semaphore->try_wait( );
			}
			return true;
		}
	};

	namespace impl {
		template<typename... Tasks>
		constexpr bool are_tasks_v = daw::all_true_v<daw::is_callable_v<Tasks>...>;

		template<typename Waitable>
		using is_waitable_detector =
		  decltype( std::declval<Waitable &>( ).wait( ) );

		template<typename Waitable>
		constexpr bool is_waitable_v =
		  daw::is_detected_v<is_waitable_detector, Waitable>;

		using task_ptr_t = daw::parallel::msg_ptr_t<task_t>;

		class task_scheduler_impl;

		void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself,
		                  boost::optional<daw::shared_latch> sem );

		void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself );

		class task_scheduler_impl
		  : public std::enable_shared_from_this<task_scheduler_impl> {
			using task_queue_t =
			  daw::parallel::mpmc_msg_queue_t<daw::impl::task_ptr_t>;

			daw::lockable_value_t<std::vector<std::thread>> m_threads;
			std::atomic_bool m_continue;
			bool m_block_on_destruction;
			size_t const m_num_threads;
			std::vector<task_queue_t> m_tasks;
			std::atomic<size_t> m_task_count;
			daw::lockable_value_t<std::list<boost::optional<std::thread>>>
			  m_other_threads;

			std::weak_ptr<task_scheduler_impl> get_weak_this( );

			daw::impl::task_ptr_t wait_for_task_from_pool( size_t id );

		public:
			task_scheduler_impl( std::size_t num_threads, bool block_on_destruction );
			~task_scheduler_impl( );
			task_scheduler_impl( task_scheduler_impl && ) =
			  delete; // TODO: investigate why implicitly deleted
			task_scheduler_impl &operator=( task_scheduler_impl && ) =
			  delete; // TODO: investigate why implicitly deleted

			task_scheduler_impl( task_scheduler_impl const & ) = delete;
			task_scheduler_impl &operator=( task_scheduler_impl const & ) = delete;

		private:
			void send_task( task_ptr_t tsk, size_t id );

			template<typename Task>
			void add_task( Task &&task, size_t id ) {
				auto tsk = [wself = get_weak_this( ), task = std::forward<Task>( task ),
				            id]( ) mutable {
					if( wself.expired( ) ) {
						return;
					}
					auto self = wself.lock( );
					if( self ) {
						task( );
						while( self->m_continue && self->run_next_task( id ) ) {}
					}
				};
				send_task( task_ptr_t( std::move( tsk ) ), id );
			}

			template<typename Task>
			void add_task( Task &&task, daw::shared_latch sem,
			               size_t id ) {
				auto tsk = [wself = get_weak_this( ), task = std::forward<Task>( task ),
				            id]( ) mutable {
					if( wself.expired( ) ) {
						return;
					}
					auto self = wself.lock( );
					if( self ) {
						task( );
						while( self->m_continue && self->run_next_task( id ) ) {}
					}
				};
				send_task( task_ptr_t( std::move( tsk ), std::move( sem ) ), id );
			}

			void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself );
			void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself,
			                  boost::optional<daw::shared_latch> sem );

			void run_task( task_ptr_t &&tsk ) noexcept;

		public:
			template<typename Task>
			void add_task( Task &&task ) {
				static_assert(
				  daw::is_callable_v<Task>,
				  "Task must be callable without arguments (e.g. task( );)" );
				size_t id = ( m_task_count++ ) % m_num_threads;
				add_task( std::forward<Task>( task ), id );
			}

			template<typename Task>
			void add_task( Task &&task, daw::shared_latch sem ) {
				static_assert(
				  daw::is_callable_v<Task>,
				  "Task must be callable without arguments (e.g. task( );)" );
				size_t id = ( m_task_count++ ) % m_num_threads;
				add_task( std::forward<Task>( task ), std::move( sem ), id );
			}

			bool run_next_task( size_t id );

			void start( );
			void stop( bool block = true ) noexcept;
			bool started( ) const;

			size_t size( ) const {
				return m_tasks.size( );
			}
			bool am_i_in_pool( ) const noexcept;

			daw::shared_latch
			start_temp_task_runners( size_t task_count = 1 );
		}; // task_scheduler_impl

	} // namespace impl

} // namespace daw
