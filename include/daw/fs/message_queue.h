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

#include <array>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <utility>

#include <daw/fs/impl/dbg_proxy.h>
#include <daw/parallel/daw_locked_value.h>
#include <daw/parallel/daw_shared_mutex.h>

#include "impl/dbg_proxy.h"

namespace daw::parallel {
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

	template<typename T, size_t Sz>
	class locking_circular_buffer {
#ifdef __cpp_lib_thread_hardware_interference_size
		inline static constepxr size_t cache_size =
		  ::std::hardware_destructive_interference_size;
#else
		inline static constexpr size_t cache_size = 128U; // safe default
#endif

		static_assert( ::std::is_move_constructible_v<T> );
		static_assert( ::std::is_move_assignable_v<T> );
		// TODO no throw static asserts
		static_assert( Sz >= 2 );
		using value_type = T;
		struct locking_circular_buffer_impl {
			char m_front_padding[cache_size];
			value_type *const m_values =
			  static_cast<value_type *>( ::std::malloc( sizeof( value_type ) * Sz ) );

			alignas( cache_size )::std::atomic_size_t m_front = 0;
			alignas( cache_size )::std::atomic_size_t m_back = 0;

			char m_back_padding[cache_size - sizeof( ::std::atomic_size_t )];

			locking_circular_buffer_impl( ) noexcept {
				if( not m_values ) {
					std::abort( );
				}
			}

			~locking_circular_buffer_impl( ) noexcept {
				if constexpr( not std::is_trivially_destructible_v<value_type> ) {
					try {
						while( m_front != m_back ) {
							m_values[m_front].~value_type( );
							if( ++m_front == Sz ) {
								m_front = 0;
							}
						}
					} catch( ... ) { std::abort( ); }
				}
				free( m_values );
			}

			locking_circular_buffer_impl( locking_circular_buffer_impl const & ) =
			  delete;
			locking_circular_buffer_impl &
			operator=( locking_circular_buffer_impl const & ) = delete;
			locking_circular_buffer_impl( locking_circular_buffer_impl && ) noexcept =
			  delete;
			locking_circular_buffer_impl &
			operator=( locking_circular_buffer_impl && ) noexcept = delete;
		};

		locking_circular_buffer_impl m_impl = locking_circular_buffer_impl( );

	public:
		locking_circular_buffer( ) noexcept = default;

		[[nodiscard]] bool empty( ) const noexcept {
			return m_impl.m_front.load( ::std::memory_order_acquire ) ==
			       m_impl.m_back.load( ::std::memory_order_acquire );
		}

		[[nodiscard]] bool full( ) const noexcept {
			auto const next_record =
			  ( m_impl.m_back.load( ::std::memory_order_acquire ) + 1 ) % Sz;

			return next_record == m_impl.m_front.load( ::std::memory_order_acquire );
		}

		[[nodiscard]] T try_pop_front( ) noexcept {
			auto const current_front =
			  m_impl.m_front.load( ::std::memory_order_relaxed );
			if( current_front == m_impl.m_back.load( ::std::memory_order_acquire ) ) {
				// queue is empty
				return {};
			}

			auto const next_front = ( current_front + 1 ) % Sz;

			auto result = ::daw::move( m_impl.m_values[current_front] );
			m_impl.m_values[current_front].~value_type( );
			m_impl.m_front.store( next_front, ::std::memory_order_release );
			return result;
		}

		template<typename Predicate>
		[[nodiscard]] T pop_front( Predicate can_continue ) noexcept {
			static_assert( std::is_invocable_v<Predicate> );
			auto result = try_pop_front( );
			while( not result and can_continue( ) ) {
				result = try_pop_front( );
			}
			return result;
		}

		template<typename Predicate, typename Duration>
		[[nodiscard]] T pop_front( Predicate can_continue,
		                           Duration timeout ) noexcept {
			static_assert( std::is_invocable_v<Predicate> );
			auto result = try_pop_front( );
			if( result ) {
				return result;
			}
			auto const end_time =
			  std::chrono::high_resolution_clock::now( ) + timeout;

			while( not result and can_continue( ) and
			       std::chrono::high_resolution_clock::now( ) < end_time ) {
				result = try_pop_front( );
			}
			return result;
		}

		[[nodiscard]] push_back_result
		try_push_back( value_type &&value ) noexcept {
			auto const current_back =
			  m_impl.m_back.load( ::std::memory_order_relaxed );
			auto const next_back = ( current_back + 1 ) % Sz;
			if( next_back != m_impl.m_front.load( ::std::memory_order_acquire ) ) {
				if constexpr( ::std::is_aggregate_v<value_type> ) {
					new( &m_impl.m_values[current_back] )
					  value_type{::daw::move( value )};
				} else {
					new( &m_impl.m_values[current_back] )
					  value_type( ::daw::move( value ) );
				}
				m_impl.m_back.store( next_back, ::std::memory_order_release );
				return push_back_result::success;
			}
			return push_back_result::failed;
		}

		template<typename Predicate>
		[[nodiscard]] push_back_result push_back( value_type &&value,
		                                          Predicate &&pred ) noexcept {
			auto result = try_push_back( ::daw::move( value ) );
			while( result == push_back_result::failed and pred( ) ) {
				result = try_push_back( ::daw::move( value ) );
			}
			return result;
		}
	};
} // namespace daw::parallel
