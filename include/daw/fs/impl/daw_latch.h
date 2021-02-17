// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/header_libraries
//

#pragma once

#include "daw_condition_variable.h"

#include <daw/atomic_wait_predicate.h>
#include <daw/cpp_17.h>
#include <daw/daw_exception.h>
#include <daw/daw_move.h>

#include <atomic>
#include <atomic_wait>
#include <cassert>
#include <ciso646>
#include <condition_variable>
#include <cstdint>
#include <memory>

namespace daw {
	template<typename>
	struct is_latch : std::false_type {};

	template<typename T>
	inline constexpr bool is_latch_v = is_latch<daw::remove_cvref_t<T>>::value;

	template<typename>
	struct is_unique_latch : std::false_type {};

	template<typename T>
	inline constexpr bool is_unique_latch_v =
	  is_unique_latch<daw::remove_cvref_t<T>>::value;

	template<typename>
	struct is_shared_latch : std::false_type {};

	template<typename T>
	inline constexpr bool is_shared_latch_v =
	  is_shared_latch<daw::remove_cvref_t<T>>::value;

	class latch {
		std::atomic<std::ptrdiff_t> m_count{ 1 };

	public:
		inline latch( ) = default;

		template<typename Integer,
		         std::enable_if_t<std::is_integral_v<daw::remove_cvref_t<Integer>>,
		                          std::nullptr_t> = nullptr>
		inline explicit latch( Integer count )
		  : m_count( static_cast<std::ptrdiff_t>( count ) ) {
			assert( count > 0 );
		}

		inline void reset( ) {
			m_count.store( 1, std::memory_order_release );
		}

		template<typename Integer, std::enable_if_t<std::is_integral_v<Integer>,
		                                            std::nullptr_t> = nullptr>
		inline void reset( Integer count ) {
			m_count.store( static_cast<std::ptrdiff_t>( count ),
			               std::memory_order_release );
		}

		inline void add_notifier( ) {
			(void)m_count.fetch_add( 1, std::memory_order_release );
		}

		inline void notify( ) {
			std::ptrdiff_t current =
			  m_count.fetch_sub( 1, std::memory_order_release );
			if( current == 0 ) {
				std::atomic_notify_all( &m_count );
			}
		}

		inline void notify_one( ) {
			std::ptrdiff_t current =
			  m_count.fetch_sub( 1, std::memory_order_release );
			if( current == 0 ) {
				std::atomic_notify_one( &m_count );
			}
		}

		inline void wait( ) const {
			std::ptrdiff_t current = m_count.load( std::memory_order_acquire );
			while( current != 0 ) {
				std::atomic_wait( &m_count, current );
				current = m_count.load( std::memory_order_acquire );
			}
		}

		template<typename Predicate>
		inline void wait( Predicate &&pred ) {
			std::ptrdiff_t current = m_count.load( std::memory_order_acquire );
			while( current != 0 and not pred( ) ) {
				daw::parallel::atomic_wait_pred( &m_count, [&]( std::ptrdiff_t value ) {
					return value == 0 and pred( );
				} );
				current = m_count.load( std::memory_order_acquire );
			}
		}

		[[nodiscard]] inline bool try_wait( ) const {
			std::ptrdiff_t const current = m_count.load( std::memory_order_acquire );
			assert( current >= 0 );
			return current == 0;
		}

		template<typename Rep, typename Period>
		inline void wait_for( std::chrono::duration<Rep, Period> const &rel_time ) {
			return wait_until( std::chrono::steady_clock::now( ) + rel_time );
		}

		template<typename Clock, typename Duration>
		inline void
		wait_until( std::chrono::time_point<Clock, Duration> timeout_time ) const {
			while( not try_wait( ) and Clock::now( ) < timeout_time ) {
				daw::parallel::atomic_wait_pred( &m_count,
				                                 [timeout_time]( std::ptrdiff_t p ) {
					                                 if( p == 0 ) {
						                                 return true;
					                                 }
					                                 return Clock::now( ) < timeout_time;
				                                 } );
			}
		}
	}; // latch

	template<>
	struct is_latch<latch> : std::true_type {};

	class unique_latch {
		std::unique_ptr<latch> m_latch = std::make_unique<latch>( );

	public:
		constexpr unique_latch( ) = default;

		template<typename Integer,
		         std::enable_if_t<std::is_integral_v<daw::remove_cvref_t<Integer>>,
		                          std::nullptr_t> = nullptr>
		explicit inline unique_latch( Integer count )
		  : m_latch( std::make_unique<latch>( count ) ) {}

		template<typename Integer,
		         std::enable_if_t<std::is_integral_v<daw::remove_cvref_t<Integer>>,
		                          std::nullptr_t> = nullptr>
		explicit inline unique_latch( Integer count, bool latched )
		  : m_latch( std::make_unique<latch>( count, latched ) ) {}

		inline class latch *release( ) {
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
		[[nodiscard]] inline decltype( auto )
		wait_for( std::chrono::duration<Rep, Period> const &rel_time ) const {
			assert( m_latch );
			return m_latch->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] inline decltype( auto ) wait_until(
		  std::chrono::time_point<Clock, Duration> const &timeout_time ) const {
			assert( m_latch );
			return m_latch->wait_until( timeout_time );
		}

		[[nodiscard]] explicit inline operator bool( ) const noexcept {
			return static_cast<bool>( m_latch );
		}
	}; // unique_latch

	template<>
	struct is_unique_latch<unique_latch> : std::true_type {};

	class shared_latch {
		std::shared_ptr<latch> m_latch = std::make_shared<latch>( );

	public:
		shared_latch( ) = default;

		template<typename Integer,
		         std::enable_if_t<std::is_integral_v<daw::remove_cvref_t<Integer>>,
		                          std::nullptr_t> = nullptr>
		explicit inline shared_latch( Integer count )
		  : m_latch( std::make_shared<latch>( count ) ) {}

		template<typename Integer,
		         std::enable_if_t<std::is_integral_v<daw::remove_cvref_t<Integer>>,
		                          std::nullptr_t> = nullptr>
		explicit inline shared_latch( Integer count, bool latched )
		  : m_latch( std::make_shared<latch>( count, latched ) ) {}

		explicit inline shared_latch( unique_latch &&other ) noexcept
		  : m_latch( other.release( ) ) {}

		inline void add_notifier( ) {
			assert( m_latch );
			m_latch->add_notifier( );
		}

		inline void notify( ) {
			assert( m_latch );
			m_latch->notify( );
		}

		inline void set_latch( ) {
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
		[[nodiscard]] inline decltype( auto )
		wait_for( std::chrono::duration<Rep, Period> const &rel_time ) const {
			assert( m_latch );
			return m_latch->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] inline decltype( auto ) wait_until(
		  std::chrono::time_point<Clock, Duration> const &timeout_time ) const {
			assert( m_latch );
			return m_latch->wait_until( timeout_time );
		}

		[[nodiscard]] inline explicit operator bool( ) const noexcept {
			return static_cast<bool>( m_latch );
		}
	}; // shared_latch

	template<>
	struct is_shared_latch<shared_latch> : std::true_type {};

	inline void wait_all( std::initializer_list<latch> semaphores ) {
		for( auto &sem : semaphores ) {
			sem.wait( );
		}
	}
} // namespace daw
