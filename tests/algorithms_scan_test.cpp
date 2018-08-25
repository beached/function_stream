// The MIT License (MIT)
//
// Copyright (c) 2017-2018 Darrell Wright
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

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <date/date.h>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include <daw/daw_benchmark.h>
#include <daw/daw_math.h>
#include <daw/daw_random.h>
#include <daw/daw_string_view.h>
#include <daw/daw_utility.h>

#define BOOST_TEST_MODULE parallel_algorithms_scan
#include <daw/boost_test.h>

#include "algorithms.h"

#include "common.h"

template<typename value_t>
void scan_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	auto a = daw::make_random_data<value_t>( SZ, -10, 10 );
	auto b = a;
	auto c = a;

	auto const reduce_function = []( auto lhs, auto rhs ) noexcept {
		volatile int x = 0;
		for( size_t n = 0; n < 50; ++n ) {
			x = x + 1;
		}
		return lhs + rhs;
	};

	auto const result_1 = daw::benchmark( [&]( ) {
		daw::algorithm::parallel::scan( a.data( ), a.data( ) + a.size( ), b.data( ),
		                                b.data( ) + b.size( ), reduce_function,
		                                ts );

		daw::do_not_optimize( b );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		std::partial_sum( a.cbegin( ), a.cend( ), c.begin( ), reduce_function );
		daw::do_not_optimize( c );
	} );
	BOOST_REQUIRE_MESSAGE(
	  std::equal( b.cbegin( ), b.cend( ), c.cbegin( ), c.cend( ) ),
	  "Wrong return value" );
	b = a;
	c = a;
	auto const result_3 = daw::benchmark( [&]( ) {
		daw::algorithm::parallel::scan( a.cbegin( ), a.cend( ), b.begin( ),
		                                b.end( ), reduce_function, ts );
		daw::do_not_optimize( b );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		std::partial_sum( a.cbegin( ), a.cend( ), c.begin( ), reduce_function );
		daw::do_not_optimize( c );
	} );
	BOOST_REQUIRE_MESSAGE(
	  std::equal( b.cbegin( ), b.cend( ), c.cbegin( ), c.cend( ) ),
	  "Wrong return value" );

	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "scan" );
}

BOOST_AUTO_TEST_CASE( scan_int64_t ) {
	std::cout << "scan tests - int64_t\n";
	scan_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		scan_test<int64_t>( n );
	}
}
