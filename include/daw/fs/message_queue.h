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

#include <daw/daw_move.h>
#include <daw/daw_utility.h>

#include <cstdlib>

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
	class mpmc_bounded_queue {
		static_assert( Sz >= 2U, "Queue must be a power of 2 at least 2 large" );
		static_assert( ( Sz & ( Sz - 1U ) ) == 0,
		               "Queue must be a power of 2 at least 2 large" );
		std::array<std::unique_ptr<T>, Sz> m_data{ };
		std::mutex m_mut{ };
		std::condition_variable m_cond_full{ };
		std::condition_variable m_cond_empty{ };
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

		[[nodiscard]] inline bool empty( ) {
			auto const lck = std::unique_lock( m_mut );
			return empty_impl( );
		}

		[[nodiscard]] inline push_back_result
		try_push_back( std::unique_ptr<T> ptr ) {
			{
				auto const lck = std::unique_lock( m_mut );
				if( full_impl( ) ) {
					return push_back_result::failed;
				}
				auto &item = m_data[m_tail++];
				assert( not item );
				item.reset( ptr.release( ) );
			}
			m_cond_empty.notify_one( );
			return push_back_result::success;
		}

		[[nodiscard]] std::unique_ptr<T> try_pop_front( ) {
			auto const lck = std::unique_lock( m_mut );
			if( empty_impl( ) ) {
				return nullptr;
			}
			auto &item = m_data[m_head++];
			assert( static_cast<bool>( item ) );
			m_cond_full.notify_one( );
			return std::move( item );
		}

		template<typename Predicate>
		[[nodiscard]] std::unique_ptr<T> pop_front( Predicate pred ) {
			static_assert( std::is_invocable_v<Predicate> );
			auto lck = std::unique_lock( m_mut );
			if( empty_impl( ) ) {
				m_cond_empty.wait( lck, [&, p = std::move( pred )]( ) mutable {
					return p( ) and not empty_impl( );
				} );
				if( empty_impl( ) ) {
					return { };
				}
			}
			auto &item = m_data[m_head++];
			assert( static_cast<bool>( item ) );
			m_cond_full.notify_one( );
			return std::move( item );
		}

		template<typename Predicate>
		[[nodiscard]] push_back_result push_back( std::unique_ptr<T> &&value,
		                                          Predicate pred ) {
			static_assert( std::is_invocable_v<Predicate> );
			auto lck = std::unique_lock( m_mut );
			if( full_impl( ) ) {
				m_cond_full.wait( lck, [&, p = std::move( pred )]( ) mutable {
					return p( ) and not full_impl( );
				} );
				if( not full_impl( ) ) {
					return push_back_result::failed;
				}
			}
			auto &item = m_data[m_tail++];
			assert( not item );
			item.reset( value.release( ) );
			m_cond_empty.notify_one( );
			return push_back_result::success;
		}
	};

	/*
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
	  alignas( cache_line_size ) cell_t *m_buffer = new cell_t[Sz]( );
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

	  [[nodiscard]] bool empty( ) const {
	    return m_enqueue_pos.load( ::std::memory_order_acquire ) ==
	           m_dequeue_pos.load( ::std::memory_order_acquire );
	  }

	  ~mpmc_bounded_queue( ) {
	    delete[] std::exchange( m_buffer, nullptr );
	  }

	  push_back_result try_push_back( T &&data ) {
	    if( not m_buffer ) {
	      return { };
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
	      return { };
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
	        return { };
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
	*/

	template<typename T, std::size_t Sz, typename Predicate>
	[[nodiscard]] inline std::unique_ptr<T>
	pop_front( mpmc_bounded_queue<T, Sz> &q, Predicate can_continue ) {
		return q.pop_front( daw::move( can_continue ) );
	}

	template<typename T, std::size_t Sz, typename Predicate>
	[[nodiscard]] inline push_back_result push_back( mpmc_bounded_queue<T, Sz> &q,
	                                                 std::unique_ptr<T> value,
	                                                 Predicate pred ) {

		return q.push_back( daw::move( value ), daw::move( pred ) );
	}

	/*
	template<typename Queue, typename Predicate>
	[[nodiscard]] auto pop_front( Queue &q, Predicate can_continue ) {
	  static_assert( std::is_invocable_v<Predicate> );
	  auto result = q.try_pop_front( );
	  while( not result and can_continue( ) ) {
	    result = q.try_pop_front( );
	  }
	  return result;
	}

	template<typename Queue, typename Predicate, typename Duration>
	[[nodiscard]] auto pop_front( Queue &q, Predicate can_continue,
	                              Duration timeout ) {
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
	[[nodiscard]] push_back_result push_back( Queue &q, std::unique_ptr<T> value,
	                                          Predicate &&pred ) {
	  auto result = q.try_push_back( new T{ ::daw::move( value ) } );
	  while( result == push_back_result::failed and pred( ) ) {
	    result = q.try_push_back( ::daw::move( value ) );
	  }
	  return result;
	}
	 */
} // namespace daw::parallel
