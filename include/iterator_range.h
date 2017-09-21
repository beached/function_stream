// The MIT License (MIT)
//
// Copyright (c) 2017 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

namespace daw {
	template<typename Iterator>
	struct iterator_range_t {
		Iterator first;
		Iterator last;

		constexpr explicit operator bool( ) const noexcept {
			return first != last;
		}

		constexpr size_t size( ) const noexcept {
			return static_cast<size_t>( std::distance( first, last ) );
		}

		constexpr bool empty( ) const noexcept {
			return first == last;
		}

		constexpr auto &operator*( ) noexcept {
			return front( );
		}

		constexpr auto const &operator*( ) const noexcept {
			return front( );
		}

		constexpr auto operator-> ( ) const noexcept {
			return &( *first );
		}

		constexpr Iterator begin( ) noexcept {
			return first;
		}

		constexpr Iterator const begin( ) const noexcept {
			return first;
		}

		constexpr Iterator const cbegin( ) const noexcept {
			return first;
		}

		constexpr Iterator end( ) noexcept {
			return last;
		}

		constexpr Iterator const end( ) const noexcept {
			return last;
		}

		constexpr Iterator const cend( ) const noexcept {
			return last;
		}

		constexpr void advance( size_t n = 1 ) noexcept {
			std::advance( first, n );
		}

		constexpr void safe_advance( size_t n = 1 ) noexcept {
			first = daw::algorithm::safe_next( first, last, n );
		}

		constexpr iterator_range_t &operator++( ) noexcept {
			std::advance( first, 1 );
			return *this;
		}

		constexpr iterator_range_t operator++(int)noexcept {
			iterator_range_t result{*this};
			++( *this );
			return result;
		}

		constexpr auto &front( ) noexcept {
			return *first;
		}

		constexpr auto const &front( ) const noexcept {
			return *first;
		}

		constexpr auto &pop_front( ) noexcept {
			return *( first++ );
		}

		constexpr auto &operator[]( size_t n ) noexcept {
			return *std::next( first, static_cast<typename std::iterator_traits<Iterator>::difference_type>( n ) );
		}

		constexpr auto const &operator[]( size_t n ) const noexcept {
			return *std::next( first, static_cast<typename std::iterator_traits<Iterator>::difference_type>( n ) );
		}

		constexpr auto &back( ) noexcept {
			return first[std::distance( first, last ) - 1];
		}

		constexpr auto const &back( ) const noexcept {
			return first[std::distance( first, last ) - 1];
		}
	};

	template<typename Iterator>
	constexpr iterator_range_t<Iterator> make_iterator_range( Iterator first, size_t sz ) noexcept {
		return iterator_range_t<Iterator>{first, std::next( first, static_cast<intmax_t>( sz ) )};
	}

	template<typename Iterator>
	constexpr iterator_range_t<Iterator> make_iterator_range( Iterator first, Iterator last ) noexcept {
		return iterator_range_t<Iterator>{first, last};
	}
} // namespace daw
