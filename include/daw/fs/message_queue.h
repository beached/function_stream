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
		static_assert( ::std::is_move_constructible_v<T> );
		static_assert( ::std::is_move_assignable_v<T> );
		using value_type = T;
		struct locking_circular_buffer_impl {
			mutable std::mutex m_mutex = std::mutex( );
			std::array<value_type, Sz> m_values{};
			size_t m_front = 0;
			size_t m_back = 0;
			std::condition_variable m_not_empty = std::condition_variable( );
			std::condition_variable m_not_full = std::condition_variable( );
			bool m_is_full = false;

			template<typename... Args>
			[[nodiscard]] decltype( auto ) unique_lock( Args &&... args ) const {
				return std::unique_lock( m_mutex, std::forward<Args>( args )... );
			}

			locking_circular_buffer_impl( ) = default;
		};

		locking_circular_buffer_impl m_impl = locking_circular_buffer_impl( );

		[[nodiscard]] bool is_empty( ) const {
			// Assumes we are in a locked area
			return ( not m_impl.m_is_full ) and ( m_impl.m_front == m_impl.m_back );
		}

		[[nodiscard]] bool is_full( ) const {
			// Assumes we are in a locked area
			return m_impl.m_is_full;
		}

		[[nodiscard]] size_t inc_front( ) {
			// Assumes we are in a locked area
			size_t result = m_impl.m_front;
			m_impl.m_front = ( m_impl.m_front + 1 ) % Sz;
			return result;
		}

		[[nodiscard]] decltype( auto ) back( ) {
			// Assumes we are in a locked area
			return m_impl.m_values[m_impl.m_back];
		}

		void inc_back( ) {
			// Assumes we are in a locked area
			m_impl.m_back = ( m_impl.m_back + 1 ) % Sz;
		}

		template<typename Value>
		void set_back( Value &&v ) {
			// Assumes we are in a locked area
			back( ) = std::forward<Value>( v );
			inc_back( );
		}

	public:
		locking_circular_buffer( ) = default;

		[[nodiscard]] bool empty( ) const noexcept {
			auto lck = m_impl.unique_lock( std::try_to_lock );
			return is_empty( );
		}

		[[nodiscard]] T try_pop_front( ) noexcept {
			try {
				auto lck = m_impl.unique_lock( std::try_to_lock );
				if( not lck.owns_lock( ) or is_empty( ) ) {
					return {};
				}
				auto result = ::daw::move( m_impl.m_values[inc_front( )] );
				m_impl.m_is_full = false;
				m_impl.m_front = ( m_impl.m_front + 1 ) % Sz;
				lck.unlock( );
				m_impl.m_not_full.notify_one( );
				return result;
			} catch( std::system_error const &se ) {
				// Error trying to lock, for now abort,
				// maybe in the future return fail
				std::cerr << "Fatal error in pop_front( )\n" << se.what( ) << '\n';
				std::abort( );
			}
		}

		[[nodiscard]] T pop_front( ) noexcept {
			try {
				auto lck = m_impl.unique_lock( );
				if( is_empty( ) ) {
					m_impl.m_not_empty.wait( lck, [&]( ) { return not is_empty( ); } );
				}

				auto result = ::daw::move( m_impl.m_values[inc_front( )] );
				m_impl.m_is_full = false;
				m_impl.m_front = ( m_impl.m_front + 1 ) % Sz;
				lck.unlock( );
				m_impl.m_not_full.notify_one( );
				return result;
			} catch( std::system_error const &se ) {
				// Error trying to lock, for now abort,
				// maybe in the future return fail
				std::cerr << "Fatal error in pop_front( )\n" << se.what( ) << '\n';
				std::abort( );
			}
		}

		template<typename Predicate, typename Duration = std::chrono::seconds>
		[[nodiscard]] T
		pop_front( Predicate can_continue,
		           Duration &&timeout = std::chrono::seconds( 1 ) ) noexcept {
			static_assert( std::is_invocable_v<Predicate> );
			try {
				auto lck = m_impl.unique_lock( );
				if( is_empty( ) ) {
					auto const can_pop = wait_impl::wait_for(
					  m_impl.m_not_empty, lck, timeout,
					  [&]( ) { return not is_empty( ); }, can_continue );
					if( not can_pop ) {
						return {};
					}
				}
				if( not can_continue( ) ) {
					return {};
				}
				auto result = ::daw::move( m_impl.m_values[m_impl.m_front] );
				m_impl.m_is_full = false;
				m_impl.m_front = ( m_impl.m_front + 1 ) % Sz;
				lck.unlock( );
				m_impl.m_not_full.notify_one( );
				return result;
			} catch( std::system_error const &se ) {
				// Error trying to lock, for now abort,
				// maybe in the future return fail
				std::cerr << "Fatal error in pop_front( )\n" << se.what( ) << '\n';
				std::abort( );
			}
		}

		template<typename Predicate, typename Duration = std::chrono::seconds>
		[[nodiscard]] bool
		push_back( T &&value, Predicate can_continue,
		           Duration &&timeout = std::chrono::seconds( 1 ) ) {

			static_assert( std::is_invocable_v<Predicate> );

			try {
				auto lck = m_impl.unique_lock( );

				if( is_full( ) ) {
					m_impl.m_not_full.wait(
					  lck, [&]( ) { return can_continue( ) and not is_full( ); } );
				}
				auto const can_push = wait_impl::wait_for(
				  m_impl.m_not_empty, lck, timeout, [&]( ) { return not is_full( ); },
				  can_continue );
				if( not can_push ) {
					return false;
				}

				set_back( ::daw::move( value ) );
				m_impl.m_is_full = m_impl.m_front == m_impl.m_back;
				bool const should_notify = not is_empty( );
				lck.unlock( );
				if( should_notify ) {
					m_impl.m_not_empty.notify_one( );
				}

				return true;
			} catch( std::system_error const &se ) {
				// Error trying to lock, for now abort,
				// maybe in the future return fail
				std::cerr << "Fatal error in pop_front( )\n" << se.what( ) << '\n';
				std::abort( );
			}
		}

		[[nodiscard]] push_back_result try_push_back( T &&value ) {
			try {
				auto lck = m_impl.unique_lock( std::try_to_lock );
				if( not lck.owns_lock( ) or is_full( ) ) {
					return push_back_result::failed;
				}
				assert( not m_impl.m_values[m_impl.m_back] );

				set_back( ::daw::move( value ) );
				m_impl.m_is_full = m_impl.m_front == m_impl.m_back;
				bool should_notify = not is_empty( );
				lck.unlock( );
				if( should_notify ) {
					m_impl.m_not_empty.notify_one( );
				}
				return push_back_result::success;
			} catch( std::system_error const &se ) {
				// Error trying to lock, for now abort,
				// maybe in the future return fail
				std::cerr << "Fatal error in pop_front( )\n" << se.what( ) << '\n';
				std::abort( );
			}
		}

		[[nodiscard]] push_back_result try_push_back( T &&value,
		                                              bool must_be_empty ) {
			try {
				auto lck = m_impl.unique_lock( std::try_to_lock );
				if( not lck.owns_lock( ) or is_full( ) or
				    ( must_be_empty and not is_empty( ) ) ) {
					return push_back_result::failed;
				}

				assert( not m_impl.m_values[m_impl.m_back] );

				set_back( ::daw::move( value ) );
				m_impl.m_is_full = m_impl.m_front == m_impl.m_back;
				auto should_notify = is_empty( );
				lck.unlock( );
				if( should_notify ) {
					m_impl.m_not_empty.notify_one( );
				}
				return push_back_result::success;
			} catch( std::system_error const &se ) {
				// Error trying to lock, for now abort,
				// maybe in the future return fail
				std::cerr << "Fatal error in pop_front( )\n" << se.what( ) << '\n';
				std::abort( );
			}
		}
	};
} // namespace daw::parallel
