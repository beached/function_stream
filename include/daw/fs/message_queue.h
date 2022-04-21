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

#include <boost/container/devector.hpp>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace daw::parallel {
#ifdef __cpp_lib_thread_hardware_interference_size
	inline static constepxr size_t cache_line_size = std::hardware_destructive_interference_size;
#else
	inline static constexpr size_t cache_line_size = 128U; // safe default
#endif

	namespace wait_impl {
		template<typename Waiter,
		         typename Rep,
		         typename Period,
		         typename Predicate,
		         typename ConditionalChecker>
		bool wait_for( Waiter &wt,
		               std::unique_lock<std::mutex> &lock,
		               std::chrono::duration<Rep, Period> const &timeout,
		               Predicate pred,
		               ConditionalChecker cond ) {

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
	class spsc_bounded_queue {

		static_assert( std::is_move_constructible_v<T> );
		static_assert( std::is_move_assignable_v<T> );
		// TODO no throw static asserts
		static_assert( Sz >= 2 );
		using value_type = T;

		struct spsc_bounded_queue_impl {
			char m_front_padding[cache_line_size];
			value_type *const m_values;
			alignas( cache_line_size ) std::atomic_size_t m_front;
			alignas( cache_line_size ) std::atomic_size_t m_back;

			char m_back_padding[cache_line_size - sizeof( std::atomic_size_t )];

			spsc_bounded_queue_impl( ) noexcept
			  : m_values( static_cast<value_type *>( std::malloc( sizeof( value_type ) * Sz ) ) )
			  , m_front( 0 )
			  , m_back( 0 ) {

				assert( m_values );
			}

			~spsc_bounded_queue_impl( ) noexcept {
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

			spsc_bounded_queue_impl( spsc_bounded_queue_impl const & ) = delete;
			spsc_bounded_queue_impl &operator=( spsc_bounded_queue_impl const & ) = delete;
			spsc_bounded_queue_impl( spsc_bounded_queue_impl && ) noexcept = delete;
			spsc_bounded_queue_impl &operator=( spsc_bounded_queue_impl && ) noexcept = delete;
		};

		spsc_bounded_queue_impl m_impl = spsc_bounded_queue_impl( );

		void construct_value_at( size_t id, value_type &&value ) noexcept {
			static_assert( std::is_nothrow_move_constructible_v<value_type> );
			assert( id < Sz );
			if constexpr( std::is_aggregate_v<value_type> ) {
				new( &m_impl.m_values[id] ) value_type{ DAW_MOVE( value ) };
			} else {
				new( &m_impl.m_values[id] ) value_type( DAW_MOVE( value ) );
			}
		}

	public:
		spsc_bounded_queue( ) noexcept = default;

		[[nodiscard]] bool empty( ) const noexcept {
			return m_impl.m_front.load( std::memory_order_acquire ) ==
			       m_impl.m_back.load( std::memory_order_acquire );
		}

		/*
		[[nodiscard]] bool full( ) const noexcept {
		  auto next_record = m_impl.m_back.load( std::memory_order_acquire ) + 1U;
		  if( next_record >= Sz ) {
		    next_record = 0;
		  }

		  return next_record == m_impl.m_front.load( std::memory_order_acquire );
		}*/

		[[nodiscard]] T try_pop_front( ) noexcept {
			auto const current_front = m_impl.m_front.load( std::memory_order_relaxed );
			if( current_front == m_impl.m_back.load( std::memory_order_acquire ) ) {
				// queue is empty
				return { };
			}

			auto next_front = current_front + 1U;
			if( next_front >= Sz ) {
				next_front = 0;
			}

			auto result = DAW_MOVE( m_impl.m_values[current_front] );
			m_impl.m_values[current_front].~value_type( );
			m_impl.m_front.store( next_front, std::memory_order_release );
			return result;
		}

		[[nodiscard]] push_back_result try_push_back( value_type &&value ) noexcept {
			auto const current_back = m_impl.m_back.load( std::memory_order_relaxed );
			auto next_back = current_back + 1U;
			if( next_back >= Sz ) {
				next_back = 0;
			}
			if( next_back != m_impl.m_front.load( std::memory_order_acquire ) ) {
				construct_value_at( current_back, DAW_MOVE( value ) );
				m_impl.m_back.store( next_back, std::memory_order_release );
				return push_back_result::success;
			}
			return push_back_result::failed;
		}
	};

	template<typename T>
	class mpmc_bounded_queue {
		mutable std::mutex m_mut{ };
		std::condition_variable m_cv{ };
		boost::container::devector<T> m_queue{ };

	public:
		mpmc_bounded_queue( mpmc_bounded_queue && ) = delete;
		mpmc_bounded_queue &operator=( mpmc_bounded_queue && ) = delete;
		mpmc_bounded_queue( mpmc_bounded_queue const & ) = delete;
		mpmc_bounded_queue &operator=( mpmc_bounded_queue const & ) = delete;

		mpmc_bounded_queue( ) {
			static constexpr auto def_cap = 4096 / sizeof( T ) > 128U ? 4096U / sizeof( T ) : 128U;
			m_queue.reserve( def_cap );
		}

		~mpmc_bounded_queue( ) {}

		[[nodiscard]] push_back_result try_push_back( T &&data ) {
			auto lck = std::unique_lock( m_mut, std::try_to_lock );
			if( not lck ) {
				return push_back_result::failed;
			}
			m_queue.push_back( DAW_MOVE( data ) );
			m_cv.notify_one( );
			return push_back_result::success;
		}

		void push_back( T &&data ) {
			auto lck = std::unique_lock( m_mut );
			m_queue.push_back( DAW_MOVE( data ) );
			m_cv.notify_one( );
		}

		[[nodiscard]] std::optional<T> try_pop_front( ) {
			auto lck = std::unique_lock( m_mut, std::try_to_lock );
			if( not lck or m_queue.empty( ) ) {
				return { };
			}
			auto result = DAW_MOVE( m_queue.front( ) );
			m_queue.pop_front( );
			return result;
		}

		[[nodiscard]] bool try_pop_front( T &value ) {
			auto lck = std::unique_lock( m_mut, std::try_to_lock );
			if( not lck or m_queue.empty( ) ) {
				return false;
			}
			value = DAW_MOVE( m_queue.front( ) );
			m_queue.pop_front( );
			return true;
		}

		void pop_front( T &value ) {
			auto lck = std::unique_lock( m_mut );
			m_cv.wait( lck, [&] { return not m_queue.empty( ); } );
			value = DAW_MOVE( m_queue.front( ) );
			m_queue.pop_front( );
		}

		[[nodiscard]] std::optional<T> pop_front( ) {
			auto lck = std::unique_lock( m_mut );
			m_cv.wait( lck, [&] { return not m_queue.empty( ); } );
			auto result = std::optional<T>( DAW_MOVE( m_queue.front( ) ) );
			m_queue.pop_front( );
			return result;
		}

		[[nodiscard]] bool empty( ) const {
			auto lck = std::unique_lock( m_mut );
			return m_queue.empty( );
		}
	};

	template<typename Queue, typename Predicate>
	[[nodiscard]] auto pop_front( Queue &q, Predicate can_continue ) noexcept {
		static_assert( std::is_invocable_v<Predicate> );
		auto result = q.try_pop_front( );
		while( not result and can_continue( ) ) {
			result = q.try_pop_front( );
		}
		return result;
	}

	template<typename Queue, typename Predicate, typename Duration>
	[[nodiscard]] auto pop_front( Queue &q, Predicate can_continue, Duration timeout ) noexcept {
		static_assert( std::is_invocable_v<Predicate> );
		auto result = q.try_pop_front( );
		if( result ) {
			return result;
		}
		auto const end_time = std::chrono::high_resolution_clock::now( ) + timeout;

		while( not result and can_continue( ) and
		       std::chrono::high_resolution_clock::now( ) < end_time ) {
			result = q.try_pop_front( );
		}
		return result;
	}

	template<typename Queue, typename T, typename Predicate>
	[[nodiscard]] push_back_result push_back( Queue &q, T &&value, Predicate &&pred ) noexcept {
		auto result = q.try_push_back( DAW_MOVE( value ) );
		while( result == push_back_result::failed and pred( ) ) {
			result = q.try_push_back( DAW_MOVE( value ) );
		}
		return result;
	}
} // namespace daw::parallel
