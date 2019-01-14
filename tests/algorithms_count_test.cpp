// The MIT License (MIT)
//
// Copyright (c) 2017-2019 Darrell Wright
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

#define BOOST_TEST_MODULE parallel_algorithms_count
#include <daw/boost_test.h>

#include "daw/fs/algorithms.h"

#include "common.h"

template<typename value_t>
void count_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	std::vector<value_t> a;
	a.resize( SZ );
	for( size_t n = 0; n < a.size( ); ++n ) {
		a[n] = static_cast<value_t>( n );
	}

	auto const pred = []( value_t val ) noexcept {
		return val % 2 == 0;
	};

	intmax_t x1 = 0;
	intmax_t x2 = 0;

	auto const result_1 = daw::benchmark( [&]( ) {
		x1 = daw::algorithm::parallel::count_if( a.cbegin( ), a.cend( ), pred );
		daw::do_not_optimize( x1 );
	} );

	auto const result_2 = daw::benchmark( [&]( ) {
		x2 = std::count_if( a.cbegin( ), a.cend( ), pred );
		daw::do_not_optimize( x2 );
	} );

	BOOST_REQUIRE_MESSAGE( x1 == x2, "Wrong return value" );

	x1 = 0;
	x2 = 0;

	auto const result_3 = daw::benchmark( [&]( ) {
		x1 = daw::algorithm::parallel::count_if( a.cbegin( ), a.cend( ), pred );
		daw::do_not_optimize( x1 );
	} );

	auto const result_4 = daw::benchmark( [&]( ) {
		x2 = std::count_if( a.cbegin( ), a.cend( ), pred );
		daw::do_not_optimize( x2 );
	} );

	BOOST_REQUIRE_MESSAGE( x1 == x2, "Wrong return value" );

	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "count" );
}

BOOST_AUTO_TEST_CASE( count_int64_t ) {
	std::cout << "count tests - int64_t\n";
	count_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		count_test<int64_t>( n );
	}
}
