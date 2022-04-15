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

#include "daw/daw_fixed_array.h"
#include "impl/task.h"
#include "message_queue.h"

#include <daw/daw_move.h>
#include <daw/daw_ring_adaptor.h>
#include <daw/daw_scope_guard.h>
#include <daw/daw_utility.h>
#include <daw/parallel/daw_latch.h>

#if defined( __has_include ) and __has_include( <version> )
#include <version>
#endif

#if defined( __cpp_lib_atomic_wait )
#include <atomic_wait>
#else
#include <daw/parallel/ithread.h>
#endif

#include <deque>
#include <exception>
#include <list>
#include <memory>
#include <thread>
#include <vector>

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
		task_wrapper( size_t, Handle, Task ) -> task_wrapper<Handle, Task>;
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
		using task_queue_t = daw::parallel::mpmc_bounded_queue<daw::task_t, 512>;

		class task_scheduler_impl
		  : std::enable_shared_from_this<task_scheduler_impl> {
			std::mutex m_threads_mutex{ };
			std::deque<daw::parallel::ithread> m_threads{ };

			std::atomic_size_t m_num_threads{ };    // from ctor
			daw::fixed_array<task_queue_t> m_tasks; // from ctor
			std::atomic_size_t m_task_count = std::atomic_size_t( 0ULL );
			std::atomic_size_t m_current_id = std::atomic_size_t( 0ULL );
			std::atomic_bool m_continue = false;
			bool m_block_on_destruction = false; // from ctor

			friend task_scheduler;

			template<typename, typename>
			friend struct daw::impl::task_wrapper;

			void stop( bool block_on_destruction );

		public:
			explicit task_scheduler_impl( std::size_t num_threads,
			                              bool block_on_destruction );
			task_scheduler_impl( task_scheduler_impl && ) = delete;
			task_scheduler_impl( task_scheduler_impl const & ) = delete;
			task_scheduler_impl &operator=( task_scheduler_impl && ) = delete;
			task_scheduler_impl &operator=( task_scheduler_impl const & ) = delete;
			~task_scheduler_impl( );
		};
		std::shared_ptr<task_scheduler_impl> m_impl = make_ts( );

		[[nodiscard]] static std::shared_ptr<task_scheduler_impl>
		make_ts( std::size_t const num_threads =
		           ::daw::parallel::ithread::hardware_concurrency( ),
		         bool block_on_destruct = true );

		[[nodiscard]] inline auto get_handle( ) {
			class handle_t {
				std::weak_ptr<task_scheduler_impl> m_handle;

				inline explicit handle_t( std::weak_ptr<task_scheduler_impl> wptr )
				  : m_handle( daw::move( wptr ) ) {}

				friend task_scheduler;
				friend std::optional<task_scheduler>;

			public:
				[[nodiscard]] inline bool expired( ) const {
					return m_handle.expired( );
				}

				[[nodiscard]] inline std::optional<task_scheduler> lock( ) const {
					if( auto lck = m_handle.lock( ); lck ) {
						return std::optional<task_scheduler>( std::in_place,
						                                      daw::move( lck ) );
					}
					return { };
				}
			};
			return handle_t( m_impl );
		}

		[[nodiscard]] std::unique_ptr<daw::task_t>
		wait_for_task_from_pool( size_t id );

		[[nodiscard]] std::unique_ptr<daw::task_t>
		wait_for_task_from_pool( size_t id, daw::shared_latch sem );

		template<typename Predicate,
		         std::enable_if_t<std::is_invocable_r_v<bool, Predicate>,
		                          std::nullptr_t> = nullptr>
		[[nodiscard]] std::unique_ptr<daw::task_t>
		wait_for_task_from_pool( size_t id, Predicate &&pred ) {
			if( not pred( ) ) {
				return nullptr;
			}
			std::size_t const sz = std::size( m_impl->m_tasks );
			std::size_t qid = id % sz;
			bool const is_tmp_worker = id != qid;

			for( auto m = ( qid + 1 ) % sz; m != qid and pred( );
			     m = ( m + 1 ) % sz ) {

				if( auto tsk = m_impl->m_tasks[m].try_pop_front( ); tsk ) {
					if( not pred( ) ) {
						return nullptr;
					}
					return tsk;
				}
			}
			if( is_tmp_worker ) {
				while( pred( ) ) {
					if( auto tsk = m_impl->m_tasks[qid].try_pop_front( ); tsk ) {
						if( not pred( ) ) {
							return nullptr;
						}
						return tsk;
					}
					qid = ( qid + 1 ) % sz;
				}
				return nullptr;
			} else {
				return pop_front( m_impl->m_tasks[qid], DAW_FWD( pred ) );
			}
		}

		[[nodiscard]] std::unique_ptr<daw::task_t>
		wait_for_task_from_pool( size_t id, daw::parallel::stop_token tok );

		[[nodiscard]] bool send_task( std::unique_ptr<daw::task_t> &&tsk,
		                              size_t id );

		template<typename Task, std::enable_if_t<std::is_invocable_v<Task>,
		                                         std::nullptr_t> = nullptr>
		[[nodiscard]] inline bool add_task( Task &&task, size_t id ) {
			return send_task( std::make_unique<daw::task_t>( impl::task_wrapper(
			                    id, get_handle( ), DAW_FWD( task ) ) ),
			                  id );
		}

		template<typename, typename>
		friend struct daw::impl::task_wrapper;

		template<typename Task>
		[[nodiscard]] bool add_task( Task &&task, daw::shared_latch sem,
		                             size_t id ) {

			return send_task(
			  std::make_unique<daw::task_t>(
			    impl::task_wrapper( id, get_handle( ), DAW_FWD( task ) ),
			    ::daw::move( sem ) ),
			  id );
		}

		void task_runner( size_t id );
		void task_runner( size_t id, daw::shared_latch &sem );
		void task_runner( size_t id, daw::parallel::stop_token token );
		void run_task( std::unique_ptr<daw::task_t> &&tsk );

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

			return add_task( DAW_FWD( task ), get_task_id( ) );
		}

		template<typename Task>
		[[nodiscard]] bool add_task( Task &&task, daw::shared_latch sem ) {
			static_assert(
			  std::is_invocable_v<Task>,
			  "Task must be callable without arguments (e.g. task( );)" );

			return add_task( DAW_FWD( task ), ::daw::move( sem ), get_task_id( ) );
		}

		[[nodiscard]] bool run_next_task( size_t id );

		void start( );
		void stop( bool block = true );

		[[nodiscard]] bool started( ) const;

		[[nodiscard]] size_t size( ) const {
			return std::size( m_impl->m_tasks );
		}

	private:
		struct temp_task_runner {
			std::unique_ptr<daw::parallel::ithread> th;
			daw::shared_latch sem;

			temp_task_runner( std::unique_ptr<daw::parallel::ithread> &&t,
			                  daw::shared_latch s )
			  : th( ::daw::move( t ) )
			  , sem( ::daw::move( s ) ) {

				assert( sem );
			}
			temp_task_runner( temp_task_runner const & ) = delete;
			temp_task_runner &operator=( temp_task_runner const & ) = delete;

			temp_task_runner( temp_task_runner && ) noexcept = default;
			temp_task_runner &operator=( temp_task_runner && ) noexcept = default;

			~temp_task_runner( ) {
				th->stop_and_wait( );
				sem.notify( );
			}
		};

		[[nodiscard]] daw::parallel::ithread start_temp_task_runner( );

		struct empty_task {
			constexpr void operator( )( ) const noexcept {}
		};

	public:
		[[nodiscard]] bool add_task( daw::shared_latch &&sem );
		[[nodiscard]] bool add_task( daw::shared_latch const &sem );

	private:
		[[nodiscard]] bool has_empty_queue( ) const;

		void add_queue( size_t n );

	public:
		template<typename Function>
		[[nodiscard]] auto wait_for_scope( Function &&func )
		  -> decltype( DAW_FWD( func )( ) ) {
			static_assert( std::is_invocable_v<Function>,
			               "Function passed to wait_for_scope must be callable "
			               "without an arugment. e.g. func( )" );

			if( not has_empty_queue( ) ) {
				add_queue( m_impl->m_num_threads++ );
			}
			auto const tmp_runner = start_temp_task_runner( );
			return DAW_FWD( func )( );
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
			wait_for_scope( wait_for_scope_helper{ DAW_FWD( waitable ) } );
		}

		[[nodiscard]] explicit operator bool( ) const {
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

		return ts.add_task( [task = daw::mutable_capture( DAW_FWD( task ) ),
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
		if( not schedule_task( sem, DAW_FWD( task ), daw::move( ts ) ) ) {
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
	/// @returns a semaphore that will request_stop waiting when all tasks
	/// complete
	template<typename... Tasks>
	[[nodiscard]] daw::shared_latch create_task_group( Tasks &&...tasks ) {
		static_assert( are_tasks_v<Tasks...>,
		               "Tasks passed to create_task_group must be callable without "
		               "an arugment. e.g. task( )" );
		auto ts = get_task_scheduler( );
		auto sem = daw::shared_latch( sizeof...( tasks ) );

		auto const st = [&]( auto &&task ) {
			if( not schedule_task( sem, DAW_FWD( task ), ts ) ) {
				// TODO, I don't like this but I don't want to change the return value
				// to express that we failed to add the task... yet
				sem.notify( );
			}
			return 0;
		};

		Unused( ( st( DAW_FWD( tasks ) ) + ... ) );
		return sem;
	}

	/// Run concurrent tasks and return when completed
	///
	/// @param tasks callable items of the form void( )
	template<typename... Tasks>
	void invoke_tasks( task_scheduler ts, Tasks &&...tasks ) {
		ts.wait_for( create_task_group( DAW_FWD( tasks )... ) );
	}

	template<typename... Tasks>
	void invoke_tasks( Tasks &&...tasks ) {
		static_assert( are_tasks_v<Tasks...>,
		               "Tasks passed to invoke_tasks must be callable without an "
		               "arugment. e.g. task( )" );
		invoke_tasks( get_task_scheduler( ), DAW_FWD( tasks )... );
	}

	template<typename Function>
	[[nodiscard]] decltype( auto )
	wait_for_scope( Function &&func, task_scheduler ts = get_task_scheduler( ) ) {
		static_assert( std::is_invocable_v<Function>,
		               "Function passed to wait_for_scope must be callable without "
		               "an arugment. e.g. func( )" );
		return ts.wait_for_scope( DAW_FWD( func ) );
	}
} // namespace daw
