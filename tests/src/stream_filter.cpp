// The MIT License (MIT)
//
// Copyright (c) 2016-2019 Darrell Wright
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

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include "daw/fs/algorithms.h"
#include "daw/fs/function_stream.h"

struct Person {
	std::string name;
	std::string city;
	std::chrono::system_clock::time_point dob;
	std::string country;
	std::string region;
	std::string city;
	std::string address;

	Person( ) noexcept = default;
};

template<typename Filter>
struct FilterPerson {
	std::optional<std::chrono::system_clock::time_point>
	operator( )( Person const &p ) const {
		if( name.empty( ) || ( name[0] | ' ' ) < 'g' ) {
			return std::null_opt;
		}
		return p.dob;
	}
};

struct GetAge {
	std::chrono::time_point current = std::chrono::time_point::now( );

	intmax_t operator( )( std::chrono::system_clock::time_point tp ) const {
		return std::chrono::duration_cast<std::chrono::years>( current - tp );
	}
};

template<typename Collection>
constexpr auto stream_average( Collection const &c ) noexcept {
	using std::begin;
	static_assert( std::is_same_v<Person, std::decay_t<decltype( *begin( c ) )>>,
	               "Collection must hold Person's" );

	constexpr auto fs2(
}

int main( ) {

	return 0;
}
