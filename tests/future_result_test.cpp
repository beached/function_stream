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

#include <cmath>
#include <exception>
#include <iostream>

#include <daw/daw_benchmark.h>
#include <daw/daw_size_literals.h>

#include "daw/fs/future_result.h"

double fib( double n ) noexcept {
	if( n <= 1 ) {
		return 1.0;
	}
	double last = 1;
	double result = 1;
	for( uintmax_t m = 2; m < n; ++m ) {
		auto tmp = result;
		result += last;
		last = tmp;
	}
	return result;
}

void future_result_test_001( ) {
	auto f1 = daw::async( fib, 500.0 );
	double const expected_value =
	  139423224561697880139724382870407283950070256587697307264108962948325571622863290691557658876222521294125.0;
	double const actual_value = f1.get( );
	daw::expecting(
	  daw::math::nearly_equal( expected_value, actual_value, 0.00000000000001 ) );
}

void future_result_test_002( ) {
	auto f1 = daw::async( fib, 6 );
	auto f2 = f1.next( fib );
	double const expected_value = 21;
	double const actual_value = f2.get( );
	daw::expecting(
	  daw::math::nearly_equal( expected_value, actual_value, 0.00000000000001 ) );
}

void future_result_test_003( ) {
	auto f2 = daw::async( fib, 6 ).next( fib ).next( fib );
	double const expected_value = 10946;
	double const actual_value = f2.get( );
	daw::expecting(
	  daw::math::nearly_equal( expected_value, actual_value, 0.00000000000001 ) );
}

void future_result_test_004( ) {
	auto count = 0;
	auto fib2 = [&count]( double d ) {
		++count;
		if( d > 200 ) {
			throw std::exception{};
		}
		return fib( d );
	};
	auto fib3 = fib2;
	auto fib4 = fib2;
	auto fib5 = fib2;

	auto f3 =
	  daw::async( fib, 6 ).next( fib2 ).next( fib3 ).next( fib4 ).next( fib5 );

	daw::expecting( f3.is_exception( ) );
	daw::expecting( 3, count );
	daw::expecting_exception( [&f3]( ) { (void)f3.get( ); } );
}

void future_result_test_005( ) {
	auto count = 0;
	auto fib2 = [&count]( double d ) {
		++count;
		if( d > 200 ) {
			throw std::exception{};
		}
		return fib( d );
	};
	auto fib3 = fib2;
	auto fib4 = fib2;
	auto fib5 = fib2;

	auto const f4 =
	  daw::async( fib, 6 ).next( fib2 ).next( fib3 ).next( fib4 ).next( fib5 );

	daw::expecting( f4.is_exception( ) );
	daw::expecting( 3, count );
	daw::expecting_exception( [&f4]( ) { (void)f4.get( ); } );
}

void future_result_test_006( ) {
	auto f5 = daw::async( []( int i ) { std::cout << i << '\n'; }, 6 )
	            .next( []( ) { return 5; } )
	            .next( []( int i ) {
		            std::cout << "done " << i << '\n';
		            return i;
	            } );

	daw::expecting( 5, f5.get( ) );
}

void future_result_test_007( ) {
	auto const f5 = daw::async( []( int i ) { std::cout << i << '\n'; }, 6 )
	                  .next( []( ) { return 5; } )
	                  .next( []( int i ) {
		                  std::cout << "done " << i << '\n';
		                  return i;
	                  } );

	daw::expecting( 5, f5.get( ) );
}

void future_result_test_008( ) {
	auto const f5 = daw::async( []( int i ) { std::cout << i << '\n'; }, 6 ) |
	                []( ) { return 5; } | []( int i ) {
		                std::cout << "done " << i << '\n';
		                return i;
	                };

	daw::expecting( 5, f5.get( ) );
}

void future_result_test_009( ) {
	auto f5 = daw::async( []( int i ) { std::cout << i << '\n'; }, 6 ) |
	          []( ) { return 5; } | []( int i ) {
		          std::cout << "done " << i << '\n';
		          return i;
	          };

	daw::expecting( 5, f5.get( ) );
}

void future_result_test_010( ) {
	auto const f5 =
	  daw::async(
	    []( int i ) {
		    std::cout << i << '\n';
		    return i * 6;
	    },
	    6 )
	    .fork( []( int i ) { return i / 6; }, []( int i ) { return i; } );

	std::get<0>( f5 ).wait( );
	std::get<1>( f5 ).wait( );
	daw::expecting( 6, std::get<0>( f5 ).get( ) );
	daw::expecting( 36, std::get<1>( f5 ).get( ) );

	auto f6 = join( f5, []( int lhs, int rhs ) { return lhs + rhs; } );
	auto result = f6.get( );
	daw::expecting( result, 42 );
}

void fork_join_test_001( ) {
	/*
	auto const f1 =
	  daw::async( []( ) { return std::string( "Hello" ); } )
	    .fork_join(
	      []( char a, char b, char c, char d, char e ) {
	        auto result = std::string( );
	        result += a;
	        result += b;
	        result += c;
	        result += d;
	        result += e;
	        return result;
	      },
	      []( std::string const &s ) -> char { return s[0] | ' '; },
	      []( std::string const &s ) -> char { return s[1] | ' '; },
	      []( std::string const &s ) -> char { return s[2] | ' '; },
	      []( std::string const &s ) -> char { return s[3] | ' '; },
	      []( std::string const &s ) -> char { return s[4] | ' '; } );

	daw::expecting( f1.get( ) = "hello" );
	      */
}

int main( ) {
	future_result_test_001( );
	future_result_test_002( );
	future_result_test_003( );
	future_result_test_004( );
	future_result_test_005( );
	future_result_test_006( );
	future_result_test_007( );
	future_result_test_008( );
	future_result_test_009( );
	future_result_test_010( );
	fork_join_test_001( );
}
