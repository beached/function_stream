// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/function_stream
//

#pragma once

#include <daw/daw_concepts.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace daw {
	struct timed_backoff_policy_t {
		[[gnu::always_inline]] bool operator( )( std::chrono::nanoseconds elapsed ) const {
			if( elapsed > std::chrono::milliseconds( 128 ) ) {
				std::this_thread::sleep_for( std::chrono::milliseconds( 8 ) );
			} else if( elapsed > std::chrono::microseconds( 64 ) ) {
				std::this_thread::sleep_for( elapsed / 2 );
			} else if( elapsed > std::chrono::microseconds( 4 ) ) {
				std::this_thread::yield( );
			} else {
				// poll
			}
			return false;
		}
	};
	inline constexpr auto timed_backoff_policy = timed_backoff_policy_t{ };

	bool
	poll_with_backoff( invocable_result<bool, std::chrono::nanoseconds> auto backoff_policy,
	                   invocable<> auto func,
	                   std::chrono::nanoseconds max_elapsed = std::chrono::nanoseconds::zero( ) ) {

		auto const start_time = std::chrono::high_resolution_clock::now( );
		int count = 0;
		while( true ) {
			if( func( ) ) {
				return true;
			}
			if( count < 64 ) {
				++count;
				continue;
			}
			auto const elapsed = std::chrono::high_resolution_clock::now( ) - start_time;
			if( elapsed >= max_elapsed ) {
				return false;
			}
			if( backoff_policy( elapsed ) ) {
				return false;
			}
		}
	}

	enum class wait_status { found, timeout };

	template<typename T, typename Rep, typename Period>
	[[nodiscard]] wait_status atomic_wait_for( std::atomic<T> const *object,
	                                           T const &old,
	                                           std::chrono::duration<Rep, Period> const &rel_time,
	                                           std::memory_order order = std::memory_order_acquire ) {
		auto const final_time = std::chrono::high_resolution_clock::now( ) + rel_time;
		auto current = atomic_load_explicit( object, std::memory_order_acquire );

		for( int tries = 0; current == old and tries < 16; ++tries ) {
			std::this_thread::yield( );
			current = atomic_load_explicit( object, order );
		}
		poll_with_backoff( timed_backoff_policy, [&]( ) {
			current = atomic_load_explicit( object, order );
			return current != old or final_time >= std::chrono::high_resolution_clock::now( );
		} );
		if( current == old ) {
			return wait_status::timeout;
		}
		return wait_status::found;
	}

	template<typename T, typename Clock, typename Duration>
	[[nodiscard]] wait_status
	atomic_wait_until( std::atomic<T> const *object,
	                   T const &old,
	                   std::chrono::time_point<Clock, Duration> const &timeout_time,
	                   std::memory_order order = std::memory_order_acquire ) {
		return daw::atomic_wait_for( object, DAW_MOVE( old ), timeout_time - Clock::now( ), order );
	}

	template<typename T>
	void atomic_wait_if( std::atomic<T> const *object,
	                     invocable_result<bool, T> auto predicate,
	                     std::memory_order order = std::memory_order_acquire ) {
		auto current = std::atomic_load_explicit( object, std::memory_order_acquire );
		while( not predicate( std::as_const( current ) ) ) {
			std::atomic_wait_explicit( object, current, order );
			current = std::atomic_load_explicit( object, std::memory_order_relaxed );
		}
	}

	template<typename T, typename Rep, typename Period>
	[[nodiscard]] wait_status
	atomic_wait_if_for( std::atomic<T> const *object,
	                    invocable_result<bool, T> auto predicate,
	                    std::chrono::duration<Rep, Period> const &rel_time,
	                    std::memory_order order = std::memory_order_acquire ) {
		auto const final_time = std::chrono::high_resolution_clock::now( ) + rel_time;
		auto current = atomic_load_explicit( object, std::memory_order_acquire );

		for( int tries = 0; not predicate( current ) and tries < 16; ++tries ) {
			std::this_thread::yield( );
			current = atomic_load_explicit( object, order );
		}
		poll_with_backoff( timed_backoff_policy, [&]( ) {
			current = atomic_load_explicit( object, order );
			return not predicate( current ) or final_time >= std::chrono::high_resolution_clock::now( );
		} );
		if( not predcicate( current ) ) {
			return wait_status::timeout;
		}
		return wait_status::found;
	}

	template<typename T, typename Clock, typename Duration>
	[[nodiscard]] wait_status
	atomic_wait_if_until( std::atomic<T> const *object,
	                      invocable_result<bool, T> auto predicate,
	                      std::chrono::time_point<Clock, Duration> const &timeout_time,
	                      std::memory_order order = std::memory_order_acquire ) {
		return daw::atomic_wait_if_for( object,
		                                DAW_MOVE( predicate ),
		                                timeout_time - Clock::now( ),
		                                order );
	}

	template<typename T>
	void atomic_wait_value( std::atomic<T> const *object,
	                        T const &value,
	                        std::memory_order order = std::memory_order_acquire ) {
		atomic_wait_if(
		  object,
		  [&value]( T const &current_value ) { return current_value == value; },
		  order );
	}

	template<typename T, typename Rep, typename Period>
	[[nodiscard]] wait_status
	atomic_wait_value_for( std::atomic<T> const *object,
	                       T const &value,
	                       std::chrono::duration<Rep, Period> const &rel_time,
	                       std::memory_order order = std::memory_order_acquire ) {
		return atomic_wait_if_for(
		  object,
		  [&value]( T const &current_value ) { return current_value == value; },
		  rel_time,
		  order );
	}

	template<typename T, typename Clock, typename Duration>
	[[nodiscard]] wait_status
	atomic_wait_value_until( std::atomic<T> const *object,
	                         T const &value,
	                         std::chrono::time_point<Clock, Duration> const &timeout_time,
	                         std::memory_order order = std::memory_order_acquire ) {
		return atomic_wait_if_until(
		  object,
		  [&value]( T const &current_value ) { return current_value == value; },
		  timeout_time,
		  order );
	}

} // namespace daw
