// The MIT License (MIT)
//
// Copyright (c) 2017-2019 Darrell Wright
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

#include "impl/daw_condition_variable.h"

#include <daw/daw_move.h>
#include <daw/daw_utility.h>

#include <boost/lockfree/queue.hpp>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <new>
#include <utility>

namespace daw::parallel {
#ifdef __cpp_lib_thread_hardware_interference_size
	inline static constepxr size_t cache_line_size =
	  ::std::hardware_destructive_interference_size;
#else
	inline static constexpr size_t cache_line_size = 128U; // safe default
#endif

	namespace wait_impl {
		template<typename Waiter, typename Rep, typename Period, typename Predicate,
		         typename ConditionalChecker>
		bool wait_for( Waiter &wt, std::unique_lock<std::mutex> &lock,
		               std::chrono::duration<Rep, Period> const &timeout,
		               Predicate pred, ConditionalChecker cond ) {

			static_assert( std::is_invocable_v<Predicate> );
			static_assert( std::is_invocable_v<ConditionalChecker> );
			auto status = wt.wait_for( lock, timeout, pred );
			while( cond( ) and not status ) {
				status = wt.wait_for( lock, timeout, pred );
			}
			return status and cond( );
		}
	} // namespace wait_impl
	enum class push_back_result : bool { failed, success };

	template<typename T, std::size_t Sz>
	class mpmc_bounded_queue {
		static_assert( Sz >= 2U, "Queue must be at least 2 large" );
		static_assert( ( Sz & ( Sz - 1U ) ) == 0, "Queue must be a power of 2" );
		boost::lockfree::queue<T *, boost::lockfree::capacity<Sz>> m_data{ };

	public:
		mpmc_bounded_queue( ) noexcept = default;

		mpmc_bounded_queue( mpmc_bounded_queue const & ) = delete;
		mpmc_bounded_queue( mpmc_bounded_queue && ) = delete;
		mpmc_bounded_queue &operator=( mpmc_bounded_queue const & ) = delete;
		mpmc_bounded_queue &operator=( mpmc_bounded_queue && ) = delete;
		~mpmc_bounded_queue( ) = default;

		[[nodiscard]] inline bool is_empty( ) const {
			return m_data.empty( );
		}

		[[nodiscard]] inline push_back_result
		try_push_back( std::unique_ptr<T> &&ptr ) {
			assert( ptr );
			if( not m_data.push( ptr.get( ) ) ) {
				return push_back_result::failed;
			}
			(void)ptr.release( );
			return push_back_result::success;
		}

		[[nodiscard]] std::unique_ptr<T> try_pop_front( ) {
			T *result = nullptr;
			if( not m_data.pop( result ) ) {
				return nullptr;
			}
			assert( result );
			return std::unique_ptr<T>( result );
		}

		template<typename Predicate>
		[[nodiscard]] std::unique_ptr<T> pop_front( Predicate &&can_continue ) {
			static_assert( std::is_invocable_v<Predicate> );
			T *result = nullptr;
			while( can_continue( ) ) {
				if( not m_data.pop( result ) ) {
					std::this_thread::yield( );
					std::this_thread::sleep_for( std::chrono::nanoseconds( 2 ) );
					result = nullptr;
					continue;
				}
				assert( result );
				return std::unique_ptr<T>( result );
			}
			return nullptr;
		}

		template<typename Predicate>
		[[nodiscard]] push_back_result push_back( std::unique_ptr<T> &&ptr,
		                                          Predicate &&can_continue ) {
			static_assert( std::is_invocable_v<Predicate> );
			assert( ptr );
			while( can_continue( ) ) {
				if( not m_data.push( ptr.get( ) ) ) {
					std::this_thread::yield( );
					std::this_thread::sleep_for( std::chrono::nanoseconds( 2 ) );
					continue;
				}
				(void)ptr.release( );
				return push_back_result::success;
			}
			return push_back_result::failed;
		}
	};

	template<typename T, std::size_t Sz, typename Predicate>
	[[nodiscard]] inline std::unique_ptr<T>
	pop_front( mpmc_bounded_queue<T, Sz> &q, Predicate &&can_continue ) {
		return q.pop_front( DAW_FWD( can_continue ) );
	}

	template<typename T, std::size_t Sz, typename Predicate>
	[[nodiscard]] inline push_back_result push_back( mpmc_bounded_queue<T, Sz> &q,
	                                                 std::unique_ptr<T> &&value,
	                                                 Predicate &&can_continue ) {

		return q.push_back( daw::move( value ), DAW_FWD( can_continue ) );
	}
} // namespace daw::parallel
