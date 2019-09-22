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

#include <deque>
#include <exception>
#include <memory>
#include <thread>
#include <vector>

#include <daw/daw_scope_guard.h>
#include <daw/daw_utility.h>
#include <daw/parallel/daw_latch.h>
#include <daw/parallel/daw_locked_value.h>

#include "impl/ithread.h"
#include "impl/task.h"
#include "message_queue.h"

namespace daw {
	namespace impl {
		template<typename Iterator, typename Handle>
		struct temp_task_runner;

		template<typename Handle, typename Task>
		struct task_wrapper {
			size_t id;
			mutable Handle wself;
			mutable Task task;

			constexpr task_wrapper( size_t Id, Handle const &hnd, Task const &tsk )
			  : id( Id )
			  , wself( hnd )
			  , task( tsk ) {}

			constexpr void operator( )( ) const {
				if( auto self = wself.lock( ); self ) {
					(void)task( );
					while( self->m_impl->m_continue and self->run_next_task( id ) ) {}
				}
			}
		};

		template<typename Handle, typename Task>
		task_wrapper( size_t, Handle, Task )->task_wrapper<Handle, Task>;
	} // namespace impl

	template<typename... Tasks>
	constexpr bool are_tasks_v = daw::all_true_v<std::is_invocable_v<Tasks>...>;

	template<typename Waitable>
	using is_waitable_detector = decltype( std::declval<Waitable &>( ).wait( ) );

	template<typename Waitable>
	constexpr bool is_waitable_v =
	  daw::is_detected_v<is_waitable_detector, std::remove_reference_t<Waitable>>;

	struct unable_to_add_task_exception : std::exception {
		unable_to_add_task_exception( ) = default;

		[[nodiscard]] char const *what( ) const noexcept override;
	};

	class task_scheduler {
		using task_queue_t = daw::parallel::mpmc_bounded_queue<::daw::task_t, 512>;

		class task_scheduler_impl {
			::daw::lockable_value_t<std::list<::daw::parallel::ithread>> m_threads =
			  ::daw::lockable_value_t<std::list<::daw::parallel::ithread>>( );
			::daw::lockable_value_t<
			  std::unordered_map<::daw::parallel::ithread::id, size_t>>
			  m_thread_map = ::daw::lockable_value_t<
			    std::unordered_map<::daw::parallel::ithread::id, size_t>>( );
			::std::atomic_size_t m_num_threads; // from ctor
			::std::deque<task_queue_t> m_tasks; // from ctor
			//::std::vector<task_queue_t> m_tasks; // from ctor
			::std::atomic_size_t m_task_count = ::std::atomic_size_t( 0ULL );
			::std::atomic_size_t m_current_id = ::std::atomic_size_t( 0ULL );
			::std::atomic_bool m_continue = false;
			bool m_block_on_destruction; // from ctor

			friend task_scheduler;

			template<typename, typename>
			friend struct ::daw::impl::task_wrapper;

			task_scheduler_impl( std::size_t num_threads, bool block_on_destruction );
			void stop( bool block_on_destruction );

		public:
			task_scheduler_impl( ) = delete;
			task_scheduler_impl( task_scheduler_impl && ) = delete;
			task_scheduler_impl( task_scheduler_impl const & ) = delete;
			task_scheduler_impl &operator=( task_scheduler_impl && ) = delete;
			task_scheduler_impl &operator=( task_scheduler_impl const & ) = delete;
			~task_scheduler_impl( );
		};

		inline static std::shared_ptr<task_scheduler_impl> make_ts(
		  size_t num_threads = ::daw::parallel::ithread::hardware_concurrency( ),
		  bool block_on_destruction = true ) {

			auto ptr = new task_scheduler_impl( num_threads, block_on_destruction );
			assert( ptr->m_tasks.size( ) == num_threads );
			return std::shared_ptr<task_scheduler_impl>( ptr );
		}

		std::shared_ptr<task_scheduler_impl> m_impl = make_ts( );

		[[nodiscard]] inline auto get_handle( ) {
			class handle_t {
				std::weak_ptr<task_scheduler_impl> m_handle;

				inline explicit handle_t( ::std::shared_ptr<task_scheduler_impl> &sptr )
				  : m_handle( sptr ) {}

				friend task_scheduler;

			public:
				[[nodiscard]] inline bool expired( ) const {
					return m_handle.expired( );
				}

				friend std::optional<task_scheduler>;
				[[nodiscard]] inline std::optional<task_scheduler> lock( ) const {
					if( auto lck = m_handle.lock( ); lck ) {
						return ::std::optional<task_scheduler>( std::in_place,
						                                        ::daw::move( lck ) );
					}
					return {};
				}
			};

			return handle_t( m_impl );
		}

		[[nodiscard]] ::daw::task_t wait_for_task_from_pool( size_t id );
		[[nodiscard]] ::daw::task_t
		wait_for_task_from_pool( size_t id, ::daw::shared_latch sem );

		[[nodiscard]] bool send_task( ::daw::task_t &&tsk, size_t id );

		template<typename Task, std::enable_if_t<std::is_invocable_v<Task>,
		                                         std::nullptr_t> = nullptr>
		[[nodiscard]] bool add_task( Task &&task, size_t id ) {
			return send_task( ::daw::task_t( impl::task_wrapper(
			                    id, get_handle( ), std::forward<Task>( task ) ) ),
			                  id );
		}

		template<typename, typename>
		friend struct ::daw::impl::task_wrapper;

		template<typename Task>
		[[nodiscard]] bool add_task( Task &&task, daw::shared_latch sem,
		                             size_t id ) {

			return send_task(
			  ::daw::task_t(
			    impl::task_wrapper( id, get_handle( ), std::forward<Task>( task ) ),
			    ::daw::move( sem ) ),
			  id );
		}

		void task_runner( size_t id );
		void task_runner( size_t id, daw::shared_latch &sem );
		void run_task( ::daw::task_t &&tsk ) noexcept;

		[[nodiscard]] size_t get_task_id( );

	public:
		inline explicit task_scheduler(
		  ::std::shared_ptr<task_scheduler_impl> &&sptr )
		  : m_impl( ::daw::move( sptr ) ) {

			assert( m_impl );
		}

		task_scheduler( );
		explicit task_scheduler( std::size_t num_threads,
		                         bool block_on_destruction = true );

		template<typename Task, std::enable_if_t<std::is_invocable_v<Task>,
		                                         std::nullptr_t> = nullptr>
		[[nodiscard]] bool add_task( Task &&task ) {
			static_assert(
			  std::is_invocable_v<Task>,
			  "Task must be callable without arguments (e.g. task( );)" );

			return add_task( std::forward<Task>( task ), get_task_id( ) );
		}

		template<typename Task>
		[[nodiscard]] bool add_task( Task &&task, daw::shared_latch sem ) {
			static_assert(
			  std::is_invocable_v<Task>,
			  "Task must be callable without arguments (e.g. task( );)" );

			return add_task( std::forward<Task>( task ), ::daw::move( sem ),
			                 get_task_id( ) );
		}

		[[nodiscard]] bool run_next_task( size_t id );

		void start( );
		void stop( bool block = true ) noexcept;

		[[nodiscard]] bool started( ) const;

		[[nodiscard]] size_t size( ) const {
			return m_impl->m_tasks.size( );
		}

	private:
		struct temp_task_runner {
			::daw::parallel::ithread th;
			::daw::shared_latch sem;

			temp_task_runner( ::daw::parallel::ithread &&t,
			                  daw::shared_latch s ) noexcept
			  : th( ::daw::move( t ) )
			  , sem( ::daw::move( s ) ) {

				assert( sem );
			}
			temp_task_runner( temp_task_runner && ) noexcept = default;
			temp_task_runner( temp_task_runner const & ) = delete;
			temp_task_runner &operator=( temp_task_runner && ) noexcept = default;
			temp_task_runner &operator=( temp_task_runner const & ) = delete;

			~temp_task_runner( ) {
				sem.notify( );
				th.join( );
			}
		};

		[[nodiscard]] temp_task_runner start_temp_task_runner( );

		struct empty_task {
			constexpr void operator( )( ) const noexcept {}
		};

	public:
		[[nodiscard]] inline decltype( auto )
		add_task( daw::shared_latch &&sem ) noexcept {
			return add_task( empty_task( ), daw::move( sem ) );
		}

		[[nodiscard]] inline decltype( auto )
		add_task( daw::shared_latch const &sem ) noexcept {
			return add_task( empty_task( ), sem );
		}

	private:
		[[nodiscard]] bool has_empty_queue( ) const;

		void add_queue( size_t n );

	public:
		template<typename Function>
		[[nodiscard]] auto wait_for_scope( Function &&func )
		  -> decltype( std::forward<Function>( func )( ) ) {
			static_assert( std::is_invocable_v<Function>,
			               "Function passed to wait_for_scope must be callable "
			               "without an arugment. e.g. func( )" );

			if( not has_empty_queue( ) ) {
				add_queue( m_impl->m_num_threads++ );
			}
			// auto const tmp_runner = start_temp_task_runner( );
			return std::forward<Function>( func )( );
		}

		template<typename Waitable>
		void wait_for( Waitable &&waitable ) {
			static_assert(
			  is_waitable_v<Waitable>,
			  "Waitable must have a wait( ) member. e.g. waitable.wait( )" );

			struct wait_for_scope_helper {
				mutable ::std::remove_reference_t<Waitable> w;

				inline void operator( )( ) const {
					w.wait( );
				}
			};
			wait_for_scope(
			  wait_for_scope_helper{std::forward<Waitable>( waitable )} );
		}

		[[nodiscard]] explicit operator bool( ) const noexcept {
			return started( );
		}
	}; // namespace daw

	task_scheduler get_task_scheduler( );

	/// Add a single task to the supplied task scheduler and notify supplied
	/// semaphore when complete
	///
	/// @param sem Semaphore to notify when task is completed
	/// @param task Task of form void( ) to run
	/// @param ts task_scheduler to add task to
	template<typename Task>
	[[nodiscard]] bool
	schedule_task( daw::shared_latch sem, Task &&task,
	               task_scheduler ts = get_task_scheduler( ) ) {
		static_assert( std::is_invocable_v<Task>,
		               "Task task passed to schedule_task must be callable without "
		               "an arugment. e.g. task( )" );

		return ts.add_task( [task =
		                       daw::mutable_capture( std::forward<Task>( task ) ),
		                     sem = daw::mutable_capture( ::daw::move( sem ) )]( ) {
			auto const at_exit = daw::on_scope_exit( [&sem]( ) { sem->notify( ); } );
			::daw::move ( *task )( );
		} );
	}

	template<typename Task>
	[[nodiscard]] daw::shared_latch
	create_waitable_task( Task &&task,
	                      task_scheduler ts = get_task_scheduler( ) ) {
		static_assert( std::is_invocable_v<Task>,
		               "Task task passed to create_waitable_task must be callable "
		               "without an arugment. "
		               "e.g. task( )" );
		auto sem = daw::shared_latch( );
		if( not schedule_task( sem, std::forward<Task>( task ),
		                       daw::move( ts ) ) ) {
			// TODO, I don't like this but I don't want to change the return value to
			// express that we failed to add the task... yet
			sem.notify( );
			std::abort( );
		}
		return sem;
	}

	/// Run concurrent tasks and return when completed
	///
	/// @param tasks callable items of the form void( )
	/// @returns a semaphore that will stop waiting when all tasks complete
	template<typename... Tasks>
	[[nodiscard]] daw::shared_latch create_task_group( Tasks &&... tasks ) {
		static_assert( are_tasks_v<Tasks...>,
		               "Tasks passed to create_task_group must be callable without "
		               "an arugment. e.g. task( )" );
		auto ts = get_task_scheduler( );
		auto sem = daw::shared_latch( sizeof...( tasks ) );

		auto const st = [&]( auto &&task ) {
			if( not schedule_task( sem, std::forward<decltype( task )>( task ),
			                       ts ) ) {
				// TODO, I don't like this but I don't want to change the return value
				// to express that we failed to add the task... yet
				sem.notify( );
			}
			return 0;
		};

		Unused( ( st( std::forward<Tasks>( tasks ) ) + ... ) );

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
		static_assert( are_tasks_v<Tasks...>,
		               "Tasks passed to invoke_tasks must be callable without an "
		               "arugment. e.g. task( )" );
		invoke_tasks( get_task_scheduler( ), std::forward<Tasks>( tasks )... );
	}

	template<typename Function>
	[[nodiscard]] decltype( auto )
	wait_for_scope( Function &&func, task_scheduler ts = get_task_scheduler( ) ) {
		static_assert( std::is_invocable_v<Function>,
		               "Function passed to wait_for_scope must be callable without "
		               "an arugment. e.g. func( )" );
		return ts.wait_for_scope( std::forward<Function>( func ) );
	}
} // namespace daw
