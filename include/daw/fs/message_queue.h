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
			return status && cond( );
		}
	} // namespace wait_impl
	enum class push_back_result : bool { failed, success };

	template<typename T, size_t Sz>
	class locking_circular_buffer {
		using value_type = T;
		struct members_t {
			std::array<value_type, Sz> m_values{};
			std::atomic_size_t m_front = 0;
			std::atomic_size_t m_back = 0;
			std::condition_variable m_not_empty = std::condition_variable( );
			std::condition_variable m_not_full = std::condition_variable( );
			std::mutex m_mutex = std::mutex( );
			bool m_is_full = false;

			members_t( ) = default;
			members_t( members_t const & ) = delete;
			members_t( members_t && ) noexcept = delete;
			members_t &operator=( members_t const & ) = delete;
			members_t &operator=( members_t && ) noexcept = delete;
			~members_t( ) = default;
		};
		::daw::dbg_proxy<members_t> m_data = std::make_unique<members_t>( );

		[[nodiscard]] bool empty( ) const {
			return ( not m_data->m_is_full ) and
			       ( m_data->m_front == m_data->m_back );
		}

		[[nodiscard]] bool full( ) const {
			return m_data->m_is_full;
		}

		[[nodiscard]] size_t inc_front( ) {
			// Assumes we are in a locked area
			size_t result = m_data->m_front;
			m_data->m_front = ( m_data->m_front + 1 ) % Sz;
			return result;
		}

	public:
		locking_circular_buffer( ) = default;

		[[nodiscard]] T try_pop_front( ) {
			auto lck = std::unique_lock( m_data->m_mutex, std::try_to_lock );
			if( not lck.owns_lock( ) or empty( ) ) {
				return {};
			}
			m_data->m_is_full = false;
			auto const pos = inc_front( );
			assert( m_data->m_values[pos] );
			return ::daw::move( m_data->m_values[pos] );
		}

		[[nodiscard]] T pop_front( ) {
			auto lck = std::unique_lock( m_data->m_mutex );
			if( empty( ) ) {
				m_data->m_not_empty.wait( lck, [&]( ) { return !empty( ); } );
			}
			auto const oe =
			  daw::on_scope_exit( [&]( ) { m_data->m_not_full.notify_one( ); } );

			m_data->m_is_full = false;
			return ::daw::move( m_data->m_values[inc_front( )] );
		}

		template<typename Predicate, typename Duration = std::chrono::seconds>
		[[nodiscard]] T
		pop_front( Predicate can_continue,
		           Duration &&timeout = std::chrono::seconds( 1 ) ) {
			static_assert( std::is_invocable_v<Predicate> );
			auto lck = std::unique_lock( m_data->m_mutex );
			if( empty( ) ) {
				auto const can_pop = wait_impl::wait_for(
				  m_data->m_not_empty, lck, timeout, [&]( ) { return not empty( ); },
				  can_continue );
				if( not can_pop ) {
					return {};
				}
			}
			if( not can_continue( ) ) {
				return {};
			}
			auto const oe = daw::on_scope_exit( [&]( ) {
				m_data->m_is_full = false;
				m_data->m_front = ( m_data->m_front + 1 ) % Sz;
				m_data->m_not_full.notify_one( );
			} );

			return ::daw::move( m_data->m_values[m_data->m_front] );
		}

		template<typename Predicate, typename Duration = std::chrono::seconds>
		[[nodiscard]] bool
		push_back( T &&value, Predicate can_continue,
		           Duration &&timeout = std::chrono::seconds( 1 ) ) {

			static_assert( std::is_invocable_v<Predicate> );

			auto lck = std::unique_lock( m_data->m_mutex );
			if( full( ) ) {
				m_data->m_not_full.wait(
				  lck, [&]( ) { return can_continue( ) and !full( ); } );
			}
			auto const can_push = wait_impl::wait_for(
			  m_data->m_not_empty, lck, timeout, [&]( ) { return not full( ); },
			  can_continue );
			if( not can_push ) {
				return false;
			}
			auto const oe = daw::on_scope_exit( [&]( ) {
				m_data->m_is_full = m_data->m_front == m_data->m_back;
				m_data->m_not_empty.notify_one( );
			} );

			m_data->m_values[m_data->m_back] = ::daw::move( value );
			m_data->m_back = ( m_data->m_back + 1 ) % Sz;
			return true;
		}

		[[nodiscard]] push_back_result try_push_back( T &&value ) {
			auto lck = std::unique_lock( m_data->m_mutex, std::try_to_lock );
			if( !lck.owns_lock( ) or full( ) ) {
				return push_back_result::failed;
			}
			assert( not m_data->m_values[m_data->m_back] );
			auto const oe =
			  daw::on_scope_exit( [&]( ) { m_data->m_not_empty.notify_one( ); } );

			m_data->m_values[m_data->m_back] = ::daw::move( value );
			m_data->m_back = ( m_data->m_back + 1 ) % Sz;
			m_data->m_is_full = m_data->m_front == m_data->m_back;
			return push_back_result::success;
		}
	}; // namespace daw::parallel
} // namespace daw::parallel
