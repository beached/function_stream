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

	enum class wait_status { zero_count, timeout };

	template<typename T, typename Rep, typename Period>
	[[nodiscard]] wait_status wait_for( std::atomic<T> *object,
	                                    typename std::atomic<T>::value_type old,
	                                    std::chrono::duration<Rep, Period> const &rel_time ) {
		auto const final_time = std::chrono::high_resolution_clock::now( ) + rel_time;
		auto current = atomic_load( object, std::memory_order_relaxed );

		for( int tries = 0; current == old and tries < 16; ++tries ) {
			std::this_thread::yield( );
			current = atomic_load( object );
		}
		poll_with_backoff( timed_backoff_policy, [&]( ) {
			current = atomic_load( object );
			return current != old or final_time >= std::chrono::high_resolution_clock::now( );
		} );
		if( current == old ) {
			return wait_status::timeout;
		}
		return wait_status::zero_count;
	}

	template<typename T, typename Clock, typename Duration>
	[[nodiscard]] wait_status
	wait_until( std::atomic<T> *object,
	            typename std::atomic<T>::value_type old,
	            std::chrono::time_point<Clock, Duration> const &timeout_time ) {
		return daw::wait_for( object, DAW_MOVE( old ), timeout_time - Clock::now( ) );
	}
} // namespace daw
