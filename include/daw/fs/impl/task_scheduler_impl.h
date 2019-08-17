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
#include "task.h"

namespace daw::impl {
	template<typename... Tasks>
	constexpr bool are_tasks_v = daw::all_true_v<std::is_invocable_v<Tasks>...>;

	template<typename Waitable>
	using is_waitable_detector = decltype( std::declval<Waitable &>( ).wait( ) );

	template<typename Waitable>
	constexpr bool is_waitable_v =
	  daw::is_detected_v<is_waitable_detector, std::remove_reference_t<Waitable>>;

	template<typename... Args>
	inline std::optional<task_t> make_task_ptr( Args &&... args ) {
		return std::optional<task_t>( std::in_place,
		                              std::forward<Args>( args )... );
	}

	class [[nodiscard]] task_scheduler_impl {
		using task_queue_t =
		  daw::parallel::locking_circular_buffer<daw::task_t, 1024>;

		class member_data_t {
			daw::lockable_value_t<std::vector<std::thread>> m_threads{};
			daw::lockable_value_t<std::unordered_map<std::thread::id, size_t>>
			  m_thread_map{};
			bool m_block_on_destruction;       // from ctor
			size_t const m_num_threads;        // from ctor
			std::vector<task_queue_t> m_tasks; // from ctor
			std::atomic<size_t> m_task_count{0};
			daw::lockable_value_t<std::list<std::optional<std::thread>>>
			  m_other_threads{};
			std::atomic_bool m_continue = false;

			friend task_scheduler_impl;

			member_data_t( std::size_t num_threads, bool block_on_destruction );
		};
		std::shared_ptr<member_data_t> m_data;

		task_scheduler_impl( std::shared_ptr<member_data_t> && sptr )
		  : m_data( daw::move( sptr ) ) {}

		[[nodiscard]] inline auto get_handle( ) {
			class handle_t {
				std::weak_ptr<member_data_t> m_handle;

				explicit handle_t( std::weak_ptr<member_data_t> wptr )
				  : m_handle( wptr ) {}

				friend task_scheduler_impl;

			public:
				bool expired( ) const {
					return m_handle.expired( );
				}

				std::optional<task_scheduler_impl> lock( ) const {
					if( auto lck = m_handle.lock( ); lck ) {
						return task_scheduler_impl( daw::move( lck ) );
					}
					return {};
				}
			};

			return handle_t( static_cast<std::weak_ptr<member_data_t>>( m_data ) );
		}

		[[nodiscard]] std::optional<daw::task_t> wait_for_task_from_pool(
		  size_t id );

		[[nodiscard]] static std::vector<task_queue_t> make_task_queues(
		  size_t count ) {
			std::vector<task_queue_t> result{};
			result.reserve( count );
			for( size_t n = 0; n < count; ++n ) {
				result.emplace_back( );
			}
			return result;
		}

	public:
		task_scheduler_impl( std::size_t num_threads, bool block_on_destruction );

		task_scheduler_impl( task_scheduler_impl && ) = default;
		task_scheduler_impl &operator=( task_scheduler_impl && ) = default;
		task_scheduler_impl( task_scheduler_impl const & ) = default;
		task_scheduler_impl &operator=( task_scheduler_impl const & ) = default;
		~task_scheduler_impl( );

	private:
		[[nodiscard]] bool send_task( std::optional<task_t> && tsk, size_t id );

		template<typename Task>
		[[nodiscard]] bool add_task( Task && task, size_t id ) {
			return send_task(
			  make_task_ptr(
			    [wself = get_handle( ),
			     task = daw::mutable_capture( std::forward<Task>( task ) ), id]( ) {
				    if( auto self = wself.lock( ); self ) {
					    std::invoke( daw::move( *task ) );
					    while( self->m_data->m_continue and self->run_next_task( id ) ) {}
				    }
			    } ),
			  id );
		}

		template<typename Task>
		[[nodiscard]] decltype( auto ) add_task(
		  Task && task, daw::shared_latch sem, size_t id ) {
			auto tsk = [wself = get_handle( ),
			            task = daw::mutable_capture( std::forward<Task>( task ) ),
			            id]( ) {
				if( auto self = wself.lock( ); self ) {
					std::invoke( *task );
					while( self->m_data->m_continue and self->run_next_task( id ) ) {}
				}
			};
			return send_task( make_task_ptr( daw::move( tsk ), std::move( sem ) ),
			                  id );
		}

		template<typename Handle>
		void task_runner( size_t id, Handle hnd ) {

			auto self = hnd.lock( );
			if( not self ) {
				return;
			}
			auto self2 = task_scheduler_impl( *daw::move( self ) );
			while( self2.m_data->m_continue ) {
				run_task( self2.wait_for_task_from_pool( id ) );
			}
		}

		template<typename Handle>
		void task_runner( size_t id, Handle hnd,
		                  std::optional<daw::shared_latch> sem ) {

			// The self.lock( ) determines where or not the
			// task_scheduler_impl has destructed yet and keeps it alive while
			// we use members
			if( hnd.expired( ) ) {
				return;
			}
			while( not sem->try_wait( ) ) {
				if( auto self = hnd.lock( ); not( self or self->m_data->m_continue ) ) {
					return;
				} else {
					run_task( self->wait_for_task_from_pool( id ) );
				}
			}
		}

		void run_task( std::optional<task_t> && tsk ) noexcept;

		[[nodiscard]] size_t get_task_id( );

	public:
		template<typename Task>
		[[nodiscard]] decltype( auto ) add_task( Task && task ) {
			static_assert(
			  std::is_invocable_v<Task>,
			  "Task must be callable without arguments (e.g. task( );)" );

			return add_task( std::forward<Task>( task ), get_task_id( ) );
		}

		template<typename Task>
		[[nodiscard]] decltype( auto ) add_task( Task && task,
		                                         daw::shared_latch sem ) {
			static_assert(
			  std::is_invocable_v<Task>,
			  "Task must be callable without arguments (e.g. task( );)" );

			return add_task( std::forward<Task>( task ), std::move( sem ),
			                 get_task_id( ) );
		}

		[[nodiscard]] bool run_next_task( size_t id );

		void start( );
		void stop( bool block = true ) noexcept;

		[[nodiscard]] bool started( ) const;

		[[nodiscard]] size_t size( ) const {
			return m_data->m_tasks.size( );
		}

		[[nodiscard]] bool am_i_in_pool( ) const noexcept;

		[[nodiscard]] daw::shared_latch start_temp_task_runners( size_t task_count =
		                                                           1 );
	}; // task_scheduler_impl
} // namespace daw::impl
