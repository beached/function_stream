// The MIT License (MIT)
//
// Copyright (c) 2016-2018 Darrell Wright
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
#include <string>

#define BOOST_TEST_MODULE future_result
#include <daw/boost_test.h>
#include <daw/daw_benchmark.h>
#include <daw/daw_size_literals.h>

#include "future_result.h"

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

/*
BOOST_AUTO_TEST_CASE( future_result_test_001 ) {
  auto f1 = daw::make_future_result( fib, 500.0 );
  double const expected_value =
    139423224561697880139724382870407283950070256587697307264108962948325571622863290691557658876222521294125.0;
  double const actual_value = f1.get( );
  BOOST_TEST( expected_value == actual_value,
              boost::test_tools::tolerance( 0.00000000000001 ) );
}

BOOST_AUTO_TEST_CASE( future_result_test_002 ) {
  auto f1 = daw::make_future_result( fib, 6 );
  auto f2 = f1.next( fib );
  double const expected_value = 21;
  double const actual_value = f2.get( );
  BOOST_TEST( expected_value == actual_value,
              boost::test_tools::tolerance( 0.00000000000001 ) );
}

BOOST_AUTO_TEST_CASE( future_result_test_003 ) {
  auto f2 = daw::make_future_result( fib, 6 ).next( fib ).next( fib );
  double const expected_value = 10946;
  double const actual_value = f2.get( );
  BOOST_TEST( expected_value == actual_value,
              boost::test_tools::tolerance( 0.00000000000001 ) );
}
 */

BOOST_AUTO_TEST_CASE( future_result_test_004 ) {
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

	auto f3 = daw::make_future_result( fib, 6 )
	            .next( fib2 )
	            .next( fib3 )
	            .next( fib4 )
	            .next( fib5 );

	BOOST_REQUIRE( f3.is_exception( ) );
	BOOST_REQUIRE( count == 3 );
	try {
		f3.get( );
	} catch( std::exception const & ) {
		std::cout << "known exception\n";
	} catch( ... ) {
		std::cout << "unknown exception\n";
	}
}
