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
#include <iostream>
#include <random>
#include <string>

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

int a( int x ) { return x*2; }
int b( int x ) { return x*3; }
int c( int x ) { return x*4; }

struct A {
	int operator( )( int x ) const { return 1; }
};

struct B {
	int operator( )( int x ) const { return 2; }
};

struct C {
	void operator( )( std::string x ) const { }
};

struct D {
	std::string operator( )( int x ) { return std::string{ "Hello" }; }
};

int main( int, char ** ) {
	daw::impl::function_composer_t<A, B, D, C> fc{ A{ }, B{ }, D{ }, C{ } };
	static_assert( std::is_same<decltype( fc.apply( 3 ) ), decltype( C{ }( "" ) )>::value, "function_composer_t is not returning the correct type" );

	auto const fs = daw::make_function_stream( &a, &b, &c );

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(5, 7);

	auto results = daw::create_vector( fs( 3 ) );

	std::cout << fs( 1 ).get( ) << std::endl;
	/*
	for( size_t n=1; n<40; ++n ) {	
		results.push_back( fs( dis( gen ) ) );
	};
	
	for( auto const & v: results ) {
		v.get( );
		std::cout << "'" << "'\n";
	}
	*/
	return EXIT_SUCCESS;
}

