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

		explicit operator bool( ) const {
			return first != last;
		}

		constexpr size_t size( ) const noexcept {
			return static_cast<size_t>( std::distance( first, last ) );
		}

		constexpr bool empty( ) const noexcept {
			return first == last;
		}

		auto &operator*( ) {
			return front( );
		}

		auto const &operator*( ) const {
			return front( );
		}

		auto operator-> ( ) const {
			return &( *first );
		}

		Iterator begin( ) {
			return first;
		}

		Iterator const begin( ) const {
			return first;
		}

		Iterator const cbegin( ) const {
			return first;
		}

		Iterator end( ) {
			return last;
		}

		Iterator const end( ) const {
			return last;
		}

		Iterator const cend( ) const {
			return last;
		}

		void advance( size_t n = 1 ) {
			std::advance( first, n );
		}

		void safe_advance( size_t n = 1 ) {
			first = daw::algorithm::safe_next( first, last, n );
		}

		iterator_range_t &operator++( ) {
			std::advance( first, 1 );
			return *this;
		}

		iterator_range_t operator++( int ) {
			iterator_range_t result{*this};
			++( *this );
			return result;
		}

		auto &front( ) {
			return *first;
		}

		auto const &front( ) const {
			return *first;
		}

		auto &pop_front( ) {
			return *( first++ );
		}

		auto &operator[]( size_t n ) {
			return first[n];
		}

		auto const &operator[]( size_t n ) const {
			return first[n];
		}

		auto &back( ) {
			return first[std::distance( first, last ) - 1];
		}

		auto const &back( ) const {
			return first[std::distance( first, last ) - 1];
		}
	};

} // namespace daw

