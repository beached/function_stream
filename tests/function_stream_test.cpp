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

//#include <boost/multiprecision/cpp_dec_float.hpp>
#include <iostream>
#include <array>

#include <daw/daw_array.h>

#include "function_stream.h"

//using real_t = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<12500>>;
//
//real_t operator"" _R( long double d ) {
//	return real_t{ d };
//}
//
//real_t fib( real_t n ) noexcept {
//	if( 0 == n ) {
//		return 0;
//	}
//	real_t last = 0;
//	real_t result = 1;
//	for( uintmax_t m=1; m<n; ++m ) {
//		auto new_last = result;
//		result += result + last;
//		last = new_last;
//	}
//	return result;
//}
//
//real_t fib_fast( real_t const n ) {
//	// Use Binet's formula, limited n to decimal for perf increase
//	if( n < 1 ) {
//		return 0.0_R;
//	}
//	static real_t const sqrt_five = sqrt( 5.0_R );
//	static real_t const a_part = (1.0_R + sqrt_five) / 2.0_R;
//	static real_t const b_part = (1.0_R - sqrt_five) / 2.0_R;
//	
//	real_t const a = pow( a_part, n );
//	real_t const b = pow( b_part, n );
//	return round( (a - b)/sqrt_five );
//}
//
//struct doubler_t {
//	int operator( )( int x ) {
//		return x*x;
//	}
//};
//
//struct display_t {
//	static std::mutex mut;
//	auto operator( )( real_t x ) {
//		std::lock_guard<std::mutex> lck { mut };
//		std::cout << x << std::endl;
//		return x;
//	}
//};
//
//std::mutex display_t::mut { };

template<typename T, size_t Width>
struct coordinates_t {
	constexpr size_t operator( )( T x, T y ) const {
		return y*Width + x;
	}
};

int main( int, char ** ) {
	constexpr coordinates_t<size_t, 1024> to_pos;
	daw::array<intmax_t> data( to_pos( 1024, 1024 ), 1 );

	auto summer = [&data, to_pos]( auto row ) {
		auto result = data[to_pos( row, 0 )];
		for( size_t n=1; n<1024; ++n ) {
			result += data[to_pos( row, n )];			
		}
		return result;
	};

	daw::semaphore sem { -1023 };
	auto on_error = [&sem]( auto err ) {
		try {
			err.get_exception( );
		} catch( std::exception const & ex ) {
			std::cerr << "Error during function at index " << err.function_index( ) << " with message: " << ex.what( ) << std::endl;
		} catch(...) {
			std::cerr << "Unknown Error during function at index " << err.function_index( ) << std::endl;
		}
		sem.notify( );
	};


	auto const summation = daw::make_function_stream( summer, []( auto x ) { return sqrt( x );  } );
	std::array<intmax_t, 1024> result;
	for( size_t n=0; n<1024; ++n ) {
		summation( [&, idx=n]( auto x ) {
			result[idx] = x;
			sem.notify( );
		}, on_error, n );
	}

	sem.wait( );
	for( auto x: result ) {
		std::cout << x << "	";
	}
	std::cout << std::endl;
	//auto const fs = daw::make_function_stream( &fib, &fib_fast, &fib_fast );
	//
	//daw::semaphore sem { -8 };
	//auto on_error = [&sem]( auto err ) {
	//	try {
	//		err.get_exception( );
	//	} catch( std::exception const & ex ) {
	//		std::cerr << "Error during function at index " << err.function_index( ) << " with message: " << ex.what( ) << std::endl;
	//	} catch(...) {
	//		std::cerr << "Unknown Error during function at index " << err.function_index( ) << std::endl;
	//	}
	//	sem.notify( );
	//};

	//
	//auto on_complete = [&sem]( auto x ) {
	//	static std::mutex m;
	//	std::lock_guard<std::mutex> lck{ m };
	//	std::cout << "Completed->";
	//	static display_t disp;
	//	disp( x );
	//	sem.notify( );
	//};
	//
	//for( size_t n = 0; n <= 8; ++n ) {
	//	fs( on_complete, on_error, 4 );
	//}

	//sem.wait( );

	return EXIT_SUCCESS;
}

