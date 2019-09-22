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

#include <cstddef>
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

	template<typename T, size_t Sz>
	class spsc_bounded_queue {

		static_assert( ::std::is_move_constructible_v<T> );
		static_assert( ::std::is_move_assignable_v<T> );
		// TODO no throw static asserts
		static_assert( Sz >= 2 );
		using value_type = T;

		struct spsc_bounded_queue_impl {
			[[maybe_unused]] char m_front_padding[cache_line_size];
			value_type *const m_values;
			alignas( cache_line_size )::std::atomic_size_t m_front;
			alignas( cache_line_size )::std::atomic_size_t m_back;

			[[maybe_unused]] char
			  m_back_padding[cache_line_size - sizeof( ::std::atomic_size_t )];

			spsc_bounded_queue_impl( ) noexcept
			  : m_values( static_cast<value_type *>(
			      ::std::malloc( sizeof( value_type ) * Sz ) ) )
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
			spsc_bounded_queue_impl &
			operator=( spsc_bounded_queue_impl const & ) = delete;
			spsc_bounded_queue_impl( spsc_bounded_queue_impl && ) noexcept = delete;
			spsc_bounded_queue_impl &
			operator=( spsc_bounded_queue_impl && ) noexcept = delete;
		};

		spsc_bounded_queue_impl m_impl = spsc_bounded_queue_impl( );

		void construct_value_at( size_t id, value_type &&value ) noexcept {
			static_assert( ::std::is_nothrow_move_constructible_v<value_type> );
			assert( id < Sz );
			if constexpr( ::std::is_aggregate_v<value_type> ) {
				new( &m_impl.m_values[id] ) value_type{::daw::move( value )};
			} else {
				new( &m_impl.m_values[id] ) value_type( ::daw::move( value ) );
			}
		}

	public:
		spsc_bounded_queue( ) noexcept = default;

		[[nodiscard]] bool empty( ) const noexcept {
			return m_impl.m_front.load( ::std::memory_order_acquire ) ==
			       m_impl.m_back.load( ::std::memory_order_acquire );
		}

		[[nodiscard]] bool full( ) const noexcept {
			auto next_record = m_impl.m_back.load( ::std::memory_order_acquire ) + 1U;
			if( next_record >= Sz ) {
				next_record = 0;
			}

			return next_record == m_impl.m_front.load( ::std::memory_order_acquire );
		}

		[[nodiscard]] T try_pop_front( ) noexcept {
			auto const current_front =
			  m_impl.m_front.load( ::std::memory_order_relaxed );
			if( current_front == m_impl.m_back.load( ::std::memory_order_acquire ) ) {
				// queue is empty
				return {};
			}

			auto next_front = current_front + 1U;
			if( next_front >= Sz ) {
				next_front = 0;
			}

			auto result = ::daw::move( m_impl.m_values[current_front] );
			m_impl.m_values[current_front].~value_type( );
			m_impl.m_front.store( next_front, ::std::memory_order_release );
			return result;
		}

		[[nodiscard]] push_back_result
		try_push_back( value_type &&value ) noexcept {
			auto const current_back =
			  m_impl.m_back.load( ::std::memory_order_relaxed );
			auto next_back = current_back + 1U;
			if( next_back >= Sz ) {
				next_back = 0;
			}
			if( next_back != m_impl.m_front.load( ::std::memory_order_acquire ) ) {
				construct_value_at( current_back, ::daw::move( value ) );
				m_impl.m_back.store( next_back, ::std::memory_order_release );
				return push_back_result::success;
			}
			return push_back_result::failed;
		}
	};

	template<typename T, size_t Sz>
	class mpmc_bounded_queue {
		static_assert( Sz >= 2U, "Queue must be a power of 2 at least 2 large" );
		static_assert( ( Sz & ( Sz - 1U ) ) == 0,
		               "Queue must be a power of 2 at least 2 large" );
		struct cell_t {
			::std::atomic_size_t m_sequence;
			T m_data;
		};

		using cacheline_pad_t = char[cache_line_size];
		using cacheline_pad_end_t =
		  char[cache_line_size - sizeof( ::std::atomic_size_t )];

		[[maybe_unused]] alignas( cache_line_size ) cacheline_pad_t m_padding0;
		alignas( cache_line_size ) cell_t *const m_buffer = new cell_t[Sz]( );
		size_t const m_buffer_mask = Sz - 1U;
		[[maybe_unused]] cacheline_pad_t m_padding1;
		alignas( cache_line_size ) std::atomic_size_t m_enqueue_pos = 0;
		[[maybe_unused]] cacheline_pad_end_t m_padding2;
		alignas( cache_line_size ) std::atomic_size_t m_dequeue_pos = 0;
		[[maybe_unused]] cacheline_pad_end_t m_padding3;

	public:
		mpmc_bounded_queue( mpmc_bounded_queue && ) = delete;
		mpmc_bounded_queue &operator=( mpmc_bounded_queue && ) = delete;
		mpmc_bounded_queue( mpmc_bounded_queue const & ) = delete;
		mpmc_bounded_queue &operator=( mpmc_bounded_queue const & ) = delete;

		mpmc_bounded_queue( ) {
			assert( m_buffer );
			for( size_t idx = 0; idx < Sz; ++idx ) {
				m_buffer[idx].m_sequence.store( idx, std::memory_order_relaxed );
			}
		}

		[[nodiscard]] bool empty( ) const noexcept {
			return m_enqueue_pos.load( ::std::memory_order_acquire ) ==
			       m_dequeue_pos.load( ::std::memory_order_acquire );
		}

		~mpmc_bounded_queue( ) {
			delete[] m_buffer;
		}

		push_back_result try_push_back( T &&data ) {
			if( not m_buffer ) {
				return {};
			}
			cell_t *cell = nullptr;
			auto pos = m_enqueue_pos.load( std::memory_order_relaxed );
			while( true ) {
				auto const idx = pos & m_buffer_mask;
				assert( idx < Sz );
				cell = &m_buffer[idx];

				auto const seq = cell->m_sequence.load( std::memory_order_acquire );
				auto const dif =
				  static_cast<intptr_t>( seq ) - static_cast<intptr_t>( pos );
				if( dif == 0 ) {
					if( m_enqueue_pos.compare_exchange_weak(
					      pos, pos + 1, std::memory_order_relaxed ) ) {
						break;
					}
				} else if( dif < 0 ) {
					return push_back_result::failed;
				} else {
					pos = m_enqueue_pos.load( std::memory_order_relaxed );
				}
			}
			assert( cell );
			cell->m_data = ::daw::move( data );
			cell->m_sequence.store( pos + 1, std::memory_order_release );

			return push_back_result::success;
		}

		T try_pop_front( ) {
			if( not m_buffer ) {
				return {};
			}
			cell_t *cell = nullptr;
			auto pos = m_dequeue_pos.load( std::memory_order_relaxed );
			while( true ) {
				auto const idx = pos & m_buffer_mask;
				assert( idx < Sz );
				cell = &m_buffer[idx];
				auto const seq = cell->m_sequence.load( std::memory_order_acquire );

				auto const dif =
				  static_cast<intmax_t>( seq ) - static_cast<intmax_t>( pos + 1 );
				if( dif == 0 ) {
					if( m_dequeue_pos.compare_exchange_weak(
					      pos, pos + 1, std::memory_order_relaxed ) ) {
						break;
					}
				} else if( dif < 0 ) {
					return {};
				} else {
					pos = m_dequeue_pos.load( std::memory_order_relaxed );
				}
			}
			assert( cell );
			auto result = ::daw::move( cell->m_data );
			cell->m_sequence.store( pos + m_buffer_mask + 1U,
			                        std::memory_order_release );

			return result;
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
	[[nodiscard]] auto pop_front( Queue &q, Predicate can_continue,
	                              Duration timeout ) noexcept {
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
	[[nodiscard]] push_back_result push_back( Queue &q, T &&value,
	                                          Predicate &&pred ) noexcept {
		auto result = q.try_push_back( ::daw::move( value ) );
		while( result == push_back_result::failed and pred( ) ) {
			result = q.try_push_back( ::daw::move( value ) );
		}
		return result;
	}
} // namespace daw::parallel
