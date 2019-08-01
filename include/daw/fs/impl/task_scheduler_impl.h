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

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <thread>
#include <type_traits>
#include <vector>

#include <daw/daw_enable_if.h>
#include <daw/daw_traits.h>
#include <daw/parallel/daw_latch.h>
#include <daw/parallel/daw_locked_value.h>

#include "../message_queue.h"

namespace daw {
	struct task_t {
		std::function<void( )> m_function;
		daw::shared_latch m_semaphore{};

		task_t( task_t const & ) = delete;
		task_t &operator=( task_t const & ) = delete;
		task_t( task_t && ) noexcept = default;
		task_t &operator=( task_t && ) noexcept = default;
		~task_t( ) = default;

		template<typename Callable,
		         daw::enable_if_t<daw::all_true_v<
		           !daw::is_same_v<task_t, daw::remove_cvref_t<Callable>>,
		           std::is_invocable_v<daw::remove_cvref_t<Callable>>>> = nullptr>
		explicit task_t( Callable &&c )
		  : m_function( std::forward<Callable>( c ) ) {

			daw::exception::precondition_check( m_function,
			                                    "Callable must be valid" );
		}

		template<
		  typename Callable, typename SharedLatch,
		  std::enable_if_t<daw::all_true_v<std::is_invocable_v<Callable>,
		                                   daw::is_shared_latch_v<SharedLatch>>,
		                   std::nullptr_t> = nullptr>
		task_t( Callable &&c, SharedLatch &&sem )
		  : m_function( std::forward<Callable>( c ) )
		  , m_semaphore( std::forward<SharedLatch>( sem ) ) {

			daw::exception::precondition_check( m_function,
			                                    "Callable must be valid" );
		}

		void operator( )( ) noexcept( noexcept( m_function( ) ) ) {
			if( m_function ) {
				std::invoke( m_function );
			}
		}

		void operator( )( ) const noexcept( noexcept( m_function( ) ) ) {
			if( m_function ) {
				std::invoke( m_function );
			}
		}

		bool is_ready( ) const {
			if( m_semaphore ) {
				return m_semaphore.try_wait( );
			}
			return true;
		}

		explicit operator bool( ) const noexcept {
			return static_cast<bool>( m_function );
		}
	};

	namespace impl {
		template<typename... Tasks>
		constexpr bool are_tasks_v = daw::all_true_v<std::is_invocable_v<Tasks>...>;

		template<typename Waitable>
		using is_waitable_detector =
		  decltype( std::declval<Waitable &>( ).wait( ) );

		template<typename Waitable>
		constexpr bool is_waitable_v =
		  daw::is_detected_v<is_waitable_detector,
		                     std::remove_reference_t<Waitable>>;

		template<typename... Args>
		inline std::optional<task_t> make_task_ptr( Args &&... args ) {
			return std::optional<task_t>( std::in_place,
			                              std::forward<Args>( args )... );
		}

		class task_scheduler_impl;

		void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself,
		                  std::optional<daw::shared_latch> sem );

		void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself );

		class task_scheduler_impl
		  : public std::enable_shared_from_this<task_scheduler_impl> {
			using task_queue_t =
			  daw::parallel::locking_circular_buffer<daw::task_t, 1024>;

			daw::lockable_value_t<std::vector<std::thread>> m_threads;
			daw::lockable_value_t<std::unordered_map<std::thread::id, size_t>>
			  m_thread_map;
			std::shared_ptr<std::atomic_bool> m_continue =
			  std::make_shared<std::atomic_bool>( false );
			bool m_block_on_destruction;
			size_t const m_num_threads;
			std::vector<task_queue_t> m_tasks;
			std::atomic<size_t> m_task_count{0};
			daw::lockable_value_t<std::list<std::optional<std::thread>>>
			  m_other_threads{};

			std::weak_ptr<task_scheduler_impl> get_weak_this( );

			std::optional<daw::task_t> wait_for_task_from_pool( size_t id );

			static std::vector<task_queue_t> make_task_queues( size_t count ) {
				std::vector<task_queue_t> result{};
				result.reserve( count );
				for( size_t n = 0; n < count; ++n ) {
					result.emplace_back( );
				}
				return result;
			}

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
			[[nodiscard]] bool send_task( std::optional<task_t> &&tsk, size_t id );

			template<typename Task>
			[[nodiscard]] bool add_task( Task &&task, size_t id ) {
				return send_task(
				  make_task_ptr(
				    [wself = get_weak_this( ),
				     task = daw::mutable_capture( std::forward<Task>( task ) ), id]( ) {
					    if( auto self = wself.lock( ); self ) {
						    std::invoke( daw::move( *task ) );
						    while( self->m_continue and self->run_next_task( id ) ) {}
					    }
				    } ),
				  id );
			}

			template<typename Task>
			[[nodiscard]] decltype( auto )
			add_task( Task &&task, daw::shared_latch sem, size_t id ) {
				auto tsk = [wself = get_weak_this( ),
				            task = daw::mutable_capture( std::forward<Task>( task ) ),
				            id]( ) {
					if( auto self = wself.lock( ); self ) {
						std::invoke( *task );
						while( self->m_continue and self->run_next_task( id ) ) {}
					}
				};
				return send_task( make_task_ptr( daw::move( tsk ), std::move( sem ) ),
				                  id );
			}

			void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself );
			void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself,
			                  std::optional<daw::shared_latch> sem );

			void run_task( std::optional<task_t> &&tsk ) noexcept;

			[[nodiscard]] size_t get_task_id( );

		public:
			template<typename Task>
			[[nodiscard]] decltype( auto ) add_task( Task &&task ) {
				static_assert(
				  std::is_invocable_v<Task>,
				  "Task must be callable without arguments (e.g. task( );)" );

				return add_task( std::forward<Task>( task ), get_task_id( ) );
			}

			template<typename Task>
			[[nodiscard]] decltype( auto ) add_task( Task &&task,
			                                         daw::shared_latch sem ) {
				static_assert(
				  std::is_invocable_v<Task>,
				  "Task must be callable without arguments (e.g. task( );)" );

				return add_task( std::forward<Task>( task ), std::move( sem ),
				                 get_task_id( ) );
			}

			bool run_next_task( size_t id );

			void start( );
			void stop( bool block = true ) noexcept;

			[[nodiscard]] bool started( ) const;

			[[nodiscard]] size_t size( ) const {
				return m_tasks.size( );
			}

			[[nodiscard]] bool am_i_in_pool( ) const noexcept;

			[[nodiscard]] daw::shared_latch
			start_temp_task_runners( size_t task_count = 1 );
		}; // task_scheduler_impl
	}    // namespace impl
} // namespace daw
