// The MIT License (MIT)
//
// Copyright (c) 2016 Darrell Wright
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

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <cstdlib>
#include <iostream>

#include "function_stream.h"

using real_t = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<10000>>;

real_t fib( real_t n ) noexcept {
	if( 0 == n ) {
		return 0;
	}
	real_t last = 0;
	real_t result = 1;
	for( uintmax_t m=1; m<n; ++m ) {
		auto new_last = result;
		result += result + last;
		last = new_last;
	}
	return result;
}


struct doubler_t {
	int operator( )( int x ) {
		return x*x;
	}
};

struct display_t {
	void operator( )( real_t x ) {
		std::cout << x << std::endl;
	}
};

int main( int, char ** ) {
	auto fs = daw::make_function_stream( []( real_t x ) { return fib( x ); }, []( real_t x ){ return fib( x ); }, []( real_t x ) { return fib( x ); } );

	fs( display_t{ }, 5 );
	fs( display_t{ }, 3 );
	fs( display_t{ }, 13 );

	std::this_thread::sleep_for( std::chrono::minutes( 3 ) );

	return EXIT_SUCCESS;
}

