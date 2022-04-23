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
	enum class push_back_result : bool { failed, success };

	template<typename T>
	class concurrent_queue {
		mutable std::mutex m_mut{ };
		std::condition_variable m_cv{ };
		boost::container::devector<T> m_queue{ };

	public:
		concurrent_queue( concurrent_queue && ) = delete;
		concurrent_queue &operator=( concurrent_queue && ) = delete;
		concurrent_queue( concurrent_queue const & ) = delete;
		concurrent_queue &operator=( concurrent_queue const & ) = delete;

		concurrent_queue( ) {
			static constexpr auto def_cap = 4096U / sizeof( T ) > 128U ? 4096U / sizeof( T ) : 128U;
			m_queue.reserve( def_cap );
		}

		~concurrent_queue( ) {}

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