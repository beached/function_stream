// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/function_stream
//

#pragma once

#include "daw_atomic_wait.h"

#include <daw/cpp_17.h>
#include <daw/daw_concepts.h>
#include <daw/daw_cpp_feature_check.h>
#include <daw/daw_exception.h>
#include <daw/daw_move.h>
#include <daw/parallel/daw_condition_variable.h>

#include <atomic>
#include <cassert>
#include <ciso646>
#include <cstdint>
#include <memory>
#include <semaphore>
#include <thread>

namespace daw {
	template<typename>
	inline constexpr bool is_latch_v = false;

	template<typename>
	inline constexpr bool is_unique_latch_v = false;

	template<typename>
	inline constexpr bool is_shared_latch_v = false;

	class latch {
		std::atomic_int m_value = { 1 };

		inline void decrement( ) {
			assert( m_value > 0 );
			--m_value;
		}

	public:
		explicit latch( ) = default;

		explicit latch( Integer auto count )
		  : m_value( static_cast<int>( count ) ) {}

		inline void add_notifier( ) {
			atomic_fetch_add( &m_value, 1 );
		}

		inline void reset( Integer auto count ) {
			m_value.store( static_cast<int>( count ) );
		}

		inline void notify( ) {
			decrement( );
			std::atomic_notify_all( &m_value );
		}

		inline void notify_one( ) {
			decrement( );
			atomic_notify_one( &m_value );
		}

		inline void wait( ) const {
			auto old = m_value.load( std::memory_order_relaxed );
			while( old > 0 ) {
				atomic_wait( &m_value, old );
				old = atomic_load( &m_value );
			}
			assert( old == 0 );
		}

		[[nodiscard]] inline bool try_wait( ) const {
			auto const old = m_value.load( std::memory_order_relaxed );
			assert( old >= 0 );
			return old == 0;
		}

		template<typename Rep, typename Period>
		[[nodiscard]] inline wait_status
		wait_for( std::chrono::duration<Rep, Period> const &rel_time ) {
			auto const final_time = std::chrono::high_resolution_clock::now( ) + rel_time;
			auto old = m_value.load( std::memory_order_relaxed );
			if( old <= 0 ) {
				assert( old == 0 and "Unexpected negative number." );
				return wait_status::zero_count;
			}
			auto ret = atomic_wait_until( &m_value, old, final_time );
			old = m_value.load( );
			while( old > 0 and ret != wait_status::timeout ) {
				ret = atomic_wait_until( &m_value, old, final_time );
				old = m_value.load( );
			}
			assert( old == 0 and "Unexpected negative number." );
			if( old != 0 ) {
				return wait_status::timeout;
			}
			return wait_status::zero_count;
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] inline wait_status
		wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) const {
			return wait_for( timeout_time - Clock::now( ) );
		}
	}; // latch

	template<>
	inline constexpr bool is_latch_v<latch> = true;

	class unique_latch {
		std::unique_ptr<latch> m_latch = std::make_unique<latch>( );

	public:
		unique_latch( ) = default;

		explicit unique_latch( Integer auto count )
		  : m_latch( std::make_unique<latch>( count ) ) {}

		inline latch *release( ) {
			return m_latch.release( );
		}

		inline void add_notifier( ) {
			assert( m_latch );
			m_latch->add_notifier( );
		}

		inline void notify( ) {
			assert( m_latch );
			m_latch->notify( );
		}

		inline void wait( ) const {
			assert( m_latch );
			m_latch->wait( );
		}

		[[nodiscard]] inline bool try_wait( ) const {
			assert( m_latch );
			return m_latch->try_wait( );
		}

		template<typename Rep, typename Period>
		[[nodiscard]] wait_status wait_for( std::chrono::duration<Rep, Period> const &rel_time ) const {
			assert( m_latch );
			return m_latch->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] wait_status
		wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) const {
			assert( m_latch );
			return m_latch->wait_until( timeout_time );
		}

		[[nodiscard]] explicit inline operator bool( ) const noexcept {
			return static_cast<bool>( m_latch );
		}
	};

	template<>
	inline constexpr bool is_unique_latch_v<unique_latch> = true;

	class shared_latch {
		std::shared_ptr<latch> m_latch = std::make_shared<latch>( );

	public:
		shared_latch( ) = default;

		explicit shared_latch( Integer auto count )
		  : m_latch( std::make_shared<latch>( count ) ) {}

		explicit shared_latch( unique_latch &&other ) noexcept
		  : m_latch( other.release( ) ) {}

		void add_notifier( ) {
			assert( m_latch );
			m_latch->add_notifier( );
		}

		void notify( ) {
			assert( m_latch );
			m_latch->notify( );
		}

		void wait( ) const {
			assert( m_latch );
			m_latch->wait( );
		}

		[[nodiscard]] bool try_wait( ) const {
			assert( m_latch );
			return m_latch->try_wait( );
		}

		template<typename Rep, typename Period>
		[[nodiscard]] auto wait_for( std::chrono::duration<Rep, Period> const &rel_time ) {
			assert( m_latch );
			return m_latch->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] auto wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) {
			assert( m_latch );
			return m_latch->wait_until( timeout_time );
		}

		[[nodiscard]] explicit operator bool( ) const noexcept {
			return static_cast<bool>( m_latch );
		}
	};

	template<>
	inline constexpr bool is_shared_latch_v<shared_latch> = true;

	inline void wait_all( std::initializer_list<latch> semaphores ) {
		for( auto &sem : semaphores ) {
			sem.wait( );
		}
	}

	template<typename T>
	struct is_latch : std::bool_constant<is_latch_v<T>> {};

	template<typename T>
	struct is_unique_latch : std::bool_constant<is_unique_latch_v<T>> {};

	template<typename T>
	struct is_shared_latch : std::bool_constant<is_shared_latch_v<T>> {};

#if defined( __cpp_concepts )
#if __cpp_concepts >= 201907L
	template<typename T>
	concept Latch = is_latch_v<T>;

	template<typename T>
	concept UniqueLatch = is_unique_latch_v<T>;

	template<typename T>
	concept SharedLatch = is_shared_latch_v<T>;

	template<typename T>
	concept LatchTypes = SharedLatch<T> or UniqueLatch<T> or Latch<T>;
#endif
#endif

} // namespace daw
