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

#include "task_scheduler.h"
using real_t = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<10000>>;

real_t fib( uintmax_t n ) noexcept {
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

int main( int, char** ) {
	daw::task_scheduler ts;
	ts.start( );
	for( size_t n=0; n<100; ++n ) {
		ts.add_task( [n]( ) {
			std::cout << n << ": result of fib 1000 = " << fib( 1000 ) << '\n';
		} );
	}

	return EXIT_SUCCESS;
}

