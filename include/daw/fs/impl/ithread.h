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

#include "daw_latch.h"

#include <daw/daw_concepts.h>
#include <daw/daw_move.h>
#include <daw/daw_not_null.h>
#include <daw/daw_scope_guard.h>
#include <daw/daw_utility.h>

#include <memory>
#include <thread>
#include <type_traits>

namespace daw::parallel {
	class interrupt_token_owner;

	class interrupt_token {
		daw::not_null<daw::latch const *> m_condition;

		friend class daw::parallel::interrupt_token_owner;

		explicit inline interrupt_token( daw::latch const &cond ) noexcept
		  : m_condition( &cond ) {}

	public:
		[[nodiscard]] inline bool can_continue( ) const {
			return not try_wait( );
		}

		[[nodiscard]] explicit inline operator bool( ) const {
			return not try_wait( );
		}

		inline void wait( ) const {
			m_condition->wait( );
		}

		[[nodiscard]] inline bool try_wait( ) const {
			return m_condition->try_wait( );
		}

		template<typename Rep, typename Period>
		[[nodiscard]] auto wait_for( std::chrono::duration<Rep, Period> const &rel_time ) {
			return m_condition->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] auto wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) {
			m_condition->wait_until( timeout_time );
		}
	};

	class interrupt_token_owner {
		daw::latch m_condition = daw::latch( 1 );

	public:
		interrupt_token_owner( ) = default;

		~interrupt_token_owner( ) {
			stop( );
		}

		[[nodiscard]] interrupt_token get_interrupt_token( ) const noexcept {
			return interrupt_token( m_condition );
		}

		void stop( ) {
			m_condition.reset( 0 );
		}
	};

	namespace impl {
		/// Create a thread that can optionally take an interrupt token and signals when complete
		template<typename Function, typename... Args>
		std::thread
		make_thread( latch &sem, interrupt_token_owner &token, Function &&function, Args &&...args ) {
			if constexpr( std::is_invocable_v<Function, interrupt_token, Args...> ) {
				return std::thread(
				  [&sem, &token]( interrupt_token it, auto func, auto... arguments ) {
					  auto const on_exit = daw::on_scope_exit( [&sem]( ) { sem.notify( ); } );
					  return DAW_MOVE( func )( it, DAW_MOVE( arguments )... );
				  },
				  DAW_FWD( function ),
				  token.get_interrupt_token( ),
				  DAW_FWD( args )... );
			} else {
				static_assert( std::is_invocable_v<Function, Args...>,
				               "Function is not invocalle with supplied parameters." );
				return std::thread(
				  [&sem]( auto func, auto... arguments ) {
					  auto const on_exit = daw::on_scope_exit( [&sem]( ) { sem.notify( ); } );
					  return DAW_MOVE( func )( DAW_MOVE( arguments )... );
				  },
				  DAW_FWD( function ),
				  DAW_FWD( args )... );
			}
		}

		template<typename T, typename Other>
		using NotDecayOf = std::enable_if_t<not std::is_same_v<T, std::decay_t<Other>>, std::nullptr_t>;
	} // namespace impl

	class fixed_ithread {
		interrupt_token_owner m_continue{ };
		daw::latch m_sem = daw::latch( 1 );
		std::thread m_thread;

	public:
		template<typename Func, typename... Args, impl::NotDecayOf<fixed_ithread, Func> = nullptr>
		explicit fixed_ithread( Func &&func, Args &&...args )
		  : m_thread( impl::make_thread( m_sem, m_continue, DAW_FWD( func ), DAW_FWD( args )... ) ) {}

		fixed_ithread( fixed_ithread && ) = delete;
		fixed_ithread( fixed_ithread const & ) = delete;
		fixed_ithread &operator=( fixed_ithread && ) = delete;
		fixed_ithread &operator=( fixed_ithread const & ) = delete;

		inline ~fixed_ithread( ) {
			stop( );
			wait( );
			if( joinable( ) ) {
				try {
					join( );
				} catch( ... ) {
					// Do not let an exception take us down
				}
			}
		}

		[[nodiscard]] inline bool joinable( ) const {
			return m_thread.joinable( );
		}

		[[nodiscard]] inline std::thread::id get_id( ) const {
			return m_thread.get_id( );
		}

		inline void join( ) {
			m_thread.join( );
		}

		inline void wait( ) const {
			m_sem.wait( );
		}

		[[nodiscard]] inline bool try_wait( ) const {
			return m_sem.try_wait( );
		}

		template<typename Rep, typename Period>
		[[nodiscard]] auto wait_for( std::chrono::duration<Rep, Period> const &rel_time ) {
			return m_sem.wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] auto wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) {
			m_sem.wait_until( timeout_time );
		}

		inline void stop( ) {
			m_continue.stop( );
		}

		inline void detach( ) {
			m_thread.detach( );
		}

		inline void stop_and_wait( ) {
			stop( );
			wait( );
		}

		template<typename Rep, typename Period>
		[[nodiscard]] auto stop_and_wait_for( std::chrono::duration<Rep, Period> const &rel_time ) {
			stop( );
			return wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] auto
		stop_and_wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) {
			stop( );
			wait_until( timeout_time );
		}
	};

	struct ithread {
		using id = std::thread::id;

	private:
		std::unique_ptr<fixed_ithread> m_thread = { };

	public:
		ithread( ) = default;

		template<typename Function, typename... Args, impl::NotDecayOf<ithread, Function> = nullptr>
		explicit ithread( Function &&func, Args &&...args )
		  : m_thread( std::make_unique<fixed_ithread>( DAW_FWD( func ), DAW_FWD( args )... ) ) {}

		[[nodiscard]] inline bool joinable( ) const noexcept {
			assert( m_thread );
			return m_thread->joinable( );
		}

		[[nodiscard]] inline std::thread::id get_id( ) const noexcept {
			assert( m_thread );
			return m_thread->get_id( );
		}

		inline static unsigned int hardware_concurrency( ) noexcept {
			return std::thread::hardware_concurrency( );
		}

		void stop( ) {
			assert( m_thread );
			m_thread->stop( );
		}

		inline void join( ) {
			assert( m_thread );
			assert( m_thread->joinable( ) );
			m_thread->join( );
		}

		void wait( ) {
			assert( m_thread );
			m_thread->wait( );
		}

		template<typename Rep, typename Period>
		[[nodiscard]] auto wait_for( std::chrono::duration<Rep, Period> const &rel_time ) {
			assert( m_thread );
			return m_thread->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] auto wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) {
			assert( m_thread );
			m_thread->wait_until( timeout_time );
		}

		void stop_and_wait( ) {
			assert( m_thread );
			m_thread->stop_and_wait( );
		}

		template<typename Rep, typename Period>
		[[nodiscard]] auto stop_and_wait_for( std::chrono::duration<Rep, Period> const &rel_time ) {
			assert( m_thread );
			return m_thread->stop_and_wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] auto
		stop_and_wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) {
			assert( m_thread );
			m_thread->stop_and_wait_until( timeout_time );
		}

		inline void detach( ) {
			assert( m_thread );
			m_thread->detach( );
		}
	};
} // namespace daw::parallel
