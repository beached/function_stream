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
#include <cmath>
#include <iostream>
#include <random>
#include <array>

#include <daw/daw_array.h>

#include "function_stream.h"

using real_t = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<12500>>;
//using real_t = long double;

real_t operator"" _R( long double d ) {
	return real_t{ d };
}

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

real_t fib_fast( real_t const n ) {
	// Use Binet's formula, limited n to decimal for perf increase
	if( n < 1 ) {
		return 0.0_R;
	}
	static real_t const sqrt_five = sqrt( 5.0_R );
	static real_t const a_part = (1.0_R + sqrt_five) / 2.0_R;
	static real_t const b_part = (1.0_R - sqrt_five) / 2.0_R;

	real_t const a = pow( a_part, n );
	real_t const b = pow( b_part, n );
	return round( (a - b)/sqrt_five );
}

struct doubler_t {
	int operator( )( int x ) {
		return x*x;
	}
};

int main( int, char ** ) {
	auto const fs = daw::make_function_stream( &fib, &fib, &fib );
	std::random_device r;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(1, 6);

	auto results = daw::create_vector( fs( 1 ) );

	for( size_t n=1; n<100; ++n ) {	
		results.push_back( fs( dis( gen ) ) );
	};

	for( auto v: results ) {
		std::cout << v.get( ) << '\n';
	}

	return EXIT_SUCCESS;
}

