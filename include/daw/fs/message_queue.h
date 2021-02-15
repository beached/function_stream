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
		std::array<std::unique_ptr<T>, Sz> m_data{ };
		daw::condition_variable m_cond{ };
		std::size_t m_head = 0;
		std::size_t m_tail = 0;

		[[nodiscard]] inline bool empty_impl( ) const {
			return m_head == m_tail;
		}

		[[nodiscard]] inline bool full_impl( ) const {
			return ( ( m_head % Sz ) - ( m_tail % Sz ) ) == 1;
		}

	public:
		mpmc_bounded_queue( ) noexcept = default;

		mpmc_bounded_queue( mpmc_bounded_queue const & ) = delete;
		mpmc_bounded_queue( mpmc_bounded_queue && ) = delete;
		mpmc_bounded_queue &operator=( mpmc_bounded_queue const & ) = delete;
		mpmc_bounded_queue &operator=( mpmc_bounded_queue && ) = delete;
		~mpmc_bounded_queue( ) = default;

		[[nodiscard]] inline bool empty( ) {
			auto const lck = m_cond.get_lock( );
			return empty_impl( );
		}

		[[nodiscard]] inline push_back_result
		try_push_back( std::unique_ptr<T>&& ptr ) {
			assert( ptr );
			auto const lck = m_cond.get_lock( );
			if( full_impl( ) ) {
				return push_back_result::failed;
			}
			std::unique_ptr<T> &item = m_data[m_tail++];
			assert( not item );
			item.reset( ptr.release( ) );
			m_cond.notify_all( );
			return push_back_result::success;
		}

		[[nodiscard]] std::unique_ptr<T> try_pop_front( ) {
			auto const lck = m_cond.get_lock( );
			if( empty_impl( ) ) {
				return { };
			}
			std::unique_ptr<T> &item = m_data[m_head++];
			assert( static_cast<bool>( item ) );
			m_cond.notify_all( );
			return daw::move( item );
		}

		template<typename Predicate>
		[[nodiscard]] std::unique_ptr<T> pop_front( Predicate pred ) {
			static_assert( std::is_invocable_v<Predicate> );
			auto const lck =
			  m_cond.wait( [&, p = daw::mutable_capture( std::move( pred ) )]( ) {
				  return ( *p )( ) and not empty_impl( );
			  } );
			if( empty_impl( ) ) {
				return { };
			}
			std::unique_ptr<T> &item = m_data[m_head++];
			assert( item );
			m_cond.notify_all( );
			return std::move( item );
		}

		template<typename Predicate>
		[[nodiscard]] push_back_result push_back( std::unique_ptr<T> &&value,
		                                          Predicate pred ) {
			static_assert( std::is_invocable_v<Predicate> );
			auto const lck = m_cond.wait( [&, p = std::move( pred )]( ) mutable {
				return p( ) and not full_impl( );
			} );
			if( not full_impl( ) ) {
				return push_back_result::failed;
			}
			std::unique_ptr<T> &item = m_data[m_tail++];
			assert( not item );
			item = daw::move( value );
			m_cond.notify_one( );
			return push_back_result::success;
		}
	};

	template<typename T, std::size_t Sz, typename Predicate>
	[[nodiscard]] inline std::unique_ptr<T>
	pop_front( mpmc_bounded_queue<T, Sz> &q, Predicate can_continue ) {
		return q.pop_front( daw::move( can_continue ) );
	}

	template<typename T, std::size_t Sz, typename Predicate>
	[[nodiscard]] inline push_back_result push_back( mpmc_bounded_queue<T, Sz> &q,
	                                                 std::unique_ptr<T>&& value,
	                                                 Predicate pred ) {

		return q.push_back( daw::move( value ), daw::move( pred ) );
	}
} // namespace daw::parallel
