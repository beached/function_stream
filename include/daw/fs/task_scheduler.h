// The MIT License (MIT)
//
// Copyright (c) Darrell Wright
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

#include "daw_fs_concepts.h"
#include "impl/daw_latch.h"
#include "impl/ithread.h"
#include "impl/task.h"
#include "message_queue.h"

#include <daw/daw_mutable_capture.h>
#include <daw/daw_scope_guard.h>
#include <daw/parallel/daw_locked_value.h>

#include <atomic>
#include <cstddef>
#include <deque>
#include <exception>
#include <list>
#include <memory>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace daw {
	template<typename... T>
	concept Tasks = ( invocable<T> and ... );

	namespace impl {
		template<typename Iterator, typename Handle>
		struct temp_task_runner;

		template<typename Handle, invocable Function>
		struct task_wrapper {
			std::size_t id;
			mutable Handle wself;
			mutable Function func;

			explicit task_wrapper( std::size_t Id, Handle const &hnd, Function const &f )
			  : id( Id )
			  , wself( hnd )
			  , func( f ) {}

			explicit task_wrapper( std::size_t Id, Handle const &hnd, Function &&f )
			  : id( Id )
			  , wself( hnd )
			  , func( DAW_MOVE( f ) ) {}

			void operator( )( ) const {
				auto self = wself.lock( );
				if( not self ) {
					return;
				}
				(void)func( );
				while( self->m_ts_impl->m_continue and self->run_next_task( id ) ) {
					std::this_thread::yield( );
				}
			}
		};

		template<typename Handle, invocable Function>
		task_wrapper( std::size_t, Handle, Function ) -> task_wrapper<Handle, Function>;
	} // namespace impl

	struct unable_to_add_task_exception : std::exception {
		unable_to_add_task_exception( ) = default;

		[[nodiscard]] char const *what( ) const noexcept override;
	};

	class task_scheduler;
	class fixed_task_scheduler;

	std::shared_ptr<fixed_task_scheduler>
	make_shared_ts( std::size_t num_threads = daw::parallel::ithread::hardware_concurrency( ),
	                bool block_on_destruction = true );

	class handle_t {
		std::weak_ptr<fixed_task_scheduler> m_handle;

		inline explicit handle_t( std::shared_ptr<fixed_task_scheduler> &ts )
		  : m_handle( ts ) {}

		friend class daw::task_scheduler;

	public:
		[[nodiscard]] inline bool expired( ) const {
			return m_handle.expired( );
		}

		friend std::optional<task_scheduler>;
		[[nodiscard]] inline std::optional<task_scheduler> lock( ) const;
	};

	using task_queue_t = daw::parallel::concurrent_queue<unique_task_t>;

	class fixed_task_scheduler {
		daw::lockable_value_t<std::list<daw::parallel::ithread>> m_threads =
		  daw::lockable_value_t<std::list<daw::parallel::ithread>>( );
		daw::lockable_value_t<std::unordered_map<daw::parallel::ithread::id, std::size_t>>
		  m_thread_map =
		    daw::lockable_value_t<std::unordered_map<daw::parallel::ithread::id, std::size_t>>( );

		std::atomic<std::size_t> m_num_threads; // from ctor
		std::deque<task_queue_t> m_tasks;       // from ctor
		std::atomic<std::size_t> m_task_count = std::atomic<std::size_t>( 0ULL );
		std::atomic<std::size_t> m_current_id = std::atomic<std::size_t>( 0ULL );
		std::atomic<bool> m_continue = false;
		bool m_block_on_destruction; // from ctor

		/*
		friend class daw::task_scheduler;

		template<typename, invocable>
		friend struct daw::impl::task_wrapper;
		 */
		friend std::shared_ptr<fixed_task_scheduler> make_shared_ts( std::size_t, bool );

	public:
		fixed_task_scheduler( std::size_t num_threads, bool block_on_destruction );
		void stop( bool block_on_destruction );

		fixed_task_scheduler( fixed_task_scheduler && ) = delete;
		fixed_task_scheduler( fixed_task_scheduler const & ) = delete;
		fixed_task_scheduler &operator=( fixed_task_scheduler && ) = delete;
		fixed_task_scheduler &operator=( fixed_task_scheduler const & ) = delete;
		~fixed_task_scheduler( );

		[[nodiscard]] bool has_empty_queue( ) const;
		void add_queue( std::size_t n );
		[[nodiscard]] std::size_t size( ) const;

		[[nodiscard]] auto wait_for_scope( invocable auto &&func ) -> decltype( DAW_FWD( func )( ) ) {
			if( not has_empty_queue( ) ) {
				add_queue( m_num_threads++ );
			}
			return DAW_FWD( func )( );
		}

		void add_queue( std::size_t n, handle_t handle );
		[[nodiscard]] bool started( ) const;
		[[nodiscard]] unique_task_t wait_for_task_from_pool( std::size_t id );
		[[nodiscard]] unique_task_t wait_for_task_from_pool( std::size_t id, shared_cnt_sem sem );

		[[nodiscard]] bool send_task( unique_task_t tsk, std::size_t id );
		void run_task( unique_task_t tsk ) noexcept;
		[[nodiscard]] std::size_t get_task_id( );
	};

	inline std::shared_ptr<fixed_task_scheduler> make_shared_ts( std::size_t num_threads,
	                                                             bool block_on_destruction ) {

		auto ptr = new fixed_task_scheduler( num_threads, block_on_destruction );
		assert( ptr->m_tasks.size( ) == num_threads );
		return std::shared_ptr<fixed_task_scheduler>( ptr );
	}

	class task_scheduler {
		using ts_t = fixed_task_scheduler;
		friend fixed_task_scheduler;
		std::shared_ptr<ts_t> m_ts_impl = make_shared_ts( );

		[[nodiscard]] inline auto get_handle( ) {

			assert( m_ts_impl );
			return handle_t( m_ts_impl );
		}

		[[nodiscard]] unique_task_t wait_for_task_from_pool( std::size_t id );
		[[nodiscard]] unique_task_t wait_for_task_from_pool( std::size_t id, shared_cnt_sem sem );

		[[nodiscard]] bool send_task( unique_task_t tsk, std::size_t id );

		[[nodiscard]] bool add_task( invocable auto &&task, std::size_t id ) {
			return send_task( unique_task_t( impl::task_wrapper( id, get_handle( ), DAW_FWD( task ) ) ),
			                  id );
		}

		template<typename, invocable>
		friend struct daw::impl::task_wrapper;

		[[nodiscard]] bool add_task( invocable auto &&task, shared_cnt_sem sem, std::size_t id ) {

			return send_task(
			  unique_task_t( impl::task_wrapper( id, get_handle( ), DAW_FWD( task ) ), DAW_MOVE( sem ) ),
			  id );
		}

		void task_runner( std::size_t id );
		void task_runner( std::size_t id, shared_cnt_sem &sem );
		void run_task( unique_task_t tsk ) noexcept;

		[[nodiscard]] std::size_t get_task_id( );

	public:
		inline explicit task_scheduler( std::shared_ptr<ts_t> ts )
		  : m_ts_impl( DAW_MOVE( ts ) ) {

			assert( m_ts_impl );
		}

		task_scheduler( );
		explicit task_scheduler( std::size_t num_threads, bool block_on_destruction = true );

		[[nodiscard]] bool add_task( invocable auto &&task ) {
			return add_task( DAW_FWD( task ), get_task_id( ) );
		}

		[[nodiscard]] bool add_task( invocable auto &&task, shared_cnt_sem sem ) {
			return add_task( DAW_FWD( task ), DAW_MOVE( sem ), get_task_id( ) );
		}

		[[nodiscard]] bool run_next_task( std::size_t id );

		void start( );
		void stop( bool block = true ) noexcept;

		[[nodiscard]] bool started( ) const;

		[[nodiscard]] std::size_t size( ) const {
			assert( m_ts_impl );
			return m_ts_impl->size( );
		}

	private:
		struct temp_task_runner {
			daw::parallel::ithread th;
			shared_cnt_sem sem;

			temp_task_runner( daw::parallel::ithread &&t, shared_cnt_sem s ) noexcept
			  : th( DAW_MOVE( t ) )
			  , sem( DAW_MOVE( s ) ) {

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
		[[nodiscard]] inline auto add_task( shared_cnt_sem sem ) noexcept {
			return add_task( empty_task( ), DAW_MOVE( sem ) );
		}

	private:
		[[nodiscard]] bool has_empty_queue( ) const {
			assert( m_ts_impl );
			return m_ts_impl->has_empty_queue( );
		}

		void add_queue( std::size_t n );

	public:
		[[nodiscard]] auto wait_for_scope( invocable auto &&func ) -> decltype( DAW_FWD( func )( ) ) {
			assert( m_ts_impl );
			m_ts_impl->template wait_for_scope( DAW_FWD( func ) );
		}

		template<Waitable Waitable>
		void wait_for( Waitable &&waitable ) {
			struct wait_for_scope_helper {
				mutable std::remove_reference_t<Waitable> w;

				inline void operator( )( ) const {
					w.wait( );
				}
			};
			wait_for_scope( wait_for_scope_helper{ DAW_FWD( waitable ) } );
		}

		[[nodiscard]] explicit operator bool( ) const noexcept {
			return started( );
		}
	};

	std::optional<task_scheduler> handle_t::lock( ) const {
		if( auto lck = m_handle.lock( ); lck ) {
			return std::optional<task_scheduler>( std::in_place, DAW_MOVE( lck ) );
		}
		return { };
	}
	// namespace daw

	task_scheduler get_task_scheduler( );

	/// Add a single task to the supplied task scheduler and notify supplied
	/// semaphore when complete
	///
	/// @param sem Semaphore to notify when task is completed
	/// @param task Task of form void( ) to run
	/// @param ts task_scheduler to add task to
	[[nodiscard]] bool schedule_task( shared_cnt_sem sem,
	                                  invocable auto &&task,
	                                  task_scheduler ts = get_task_scheduler( ) ) {

		return ts.add_task( DAW_FWD( task ), DAW_MOVE( sem ) );
	}

	[[nodiscard]] shared_cnt_sem create_waitable_task( invocable auto &&task,
	                                                   task_scheduler ts = get_task_scheduler( ) ) {
		auto sem = shared_cnt_sem( 1 );
		auto const ae = on_scope_exit( [sem]( ) mutable { sem.notify( ); } );
		if( not schedule_task( sem, DAW_FWD( task ), DAW_MOVE( ts ) ) ) {
			throw unable_to_add_task_exception( );
		}
		return sem;
	}

	/// Run concurrent tasks and return when completed
	///
	/// @param tasks callable items of the form void( )
	/// @returns a semaphore that will stop waiting when all tasks complete
	[[nodiscard]] shared_cnt_sem create_task_group( Tasks auto &&...tasks ) {
		auto ts = get_task_scheduler( );
		auto sem = shared_cnt_sem( sizeof...( tasks ) );

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

	void invoke_tasks( Tasks auto &&...tasks ) {
		invoke_tasks( get_task_scheduler( ), DAW_FWD( tasks )... );
	}

	[[nodiscard]] decltype( auto ) wait_for_scope( invocable auto &&func,
	                                               task_scheduler ts = get_task_scheduler( ) ) {
		return ts.wait_for_scope( DAW_FWD( func ) );
	}
} // namespace daw
