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

#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace daw::parallel {
	enum class push_back_result : bool { failed, success };

	template<typename T>
	class locking_circular_buffer {
		using value_type = std::optional<T>;
		std::unique_ptr<value_type[]> m_values;
		std::unique_ptr<std::mutex> m_mutex = std::make_unique<std::mutex>( );
		size_t m_front = 0;
		size_t m_back = 0;
		size_t m_size;
		bool m_is_full = false;

		bool empty( ) const {
			return !m_is_full and m_front == m_back;
		}

		bool full( ) const {
			return m_is_full;
		}

	public:
		locking_circular_buffer( size_t queue_size )
		  : m_values( std::make_unique<value_type[]>( queue_size ) ), m_size( queue_size ) {}

		std::optional<T> pop_front( ) {
			auto lck = std::unique_lock( *m_mutex, std::try_to_lock );
			if( !lck.owns_lock( ) or empty( ) ) {
				return {};
			}
			m_is_full = false;
			assert( m_values[m_front] );
			auto result = std::exchange( m_values[m_front], std::optional<T>{} );
			m_front = ( m_front + 1 ) % m_size;
			return result;
		}

		template<typename U>
		push_back_result push_back( U &&value ) {
			static_assert( std::is_convertible_v<U, T> );
			auto lck = std::unique_lock( *m_mutex, std::try_to_lock );
			if( !lck.owns_lock( ) or full( ) ) {
				return push_back_result::failed;
			}
			assert( !m_values[m_back] );
			m_values[m_back] = std::forward<U>( value );
			m_back = ( m_back + 1 ) % m_size;
			m_is_full = m_front == m_back;
			return push_back_result::success;
		}
	};
} // namespace daw::parallel
