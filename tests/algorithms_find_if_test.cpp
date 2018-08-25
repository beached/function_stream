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

#define BOOST_TEST_MODULE parallel_algorithms_find_if
#include <daw/boost_test.h>

#include "algorithms.h"

#include "common.h"

template<typename value_t>
void find_if_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	auto a = daw::make_random_data<value_t>( SZ, -50, 50 );

	auto const pos =
	  a.size( ) - 1; // daw::randint( static_cast<size_t>( 0 ), a.size( ) );
	a[pos] = 100;
	auto const pred = []( auto const &value ) noexcept {
		return value == 100;
	};

	auto it1 = a.cend( );
	auto it2 = a.cend( );
	auto const result_1 = daw::benchmark( [&]( ) {
		it1 = daw::algorithm::parallel::find_if( a.cbegin( ), a.cend( ), pred, ts );
		daw::do_not_optimize( it1 );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		it2 = std::find_if( a.cbegin( ), a.cend( ), pred );
		daw::do_not_optimize( it2 );
	} );
	BOOST_REQUIRE_MESSAGE( it1 == it2, "Wrong return value" );

	it1 = a.cend( );
	it2 = a.cend( );
	auto const result_3 = daw::benchmark( [&]( ) {
		it1 = daw::algorithm::parallel::find_if( a.cbegin( ), a.cend( ), pred, ts );
		daw::do_not_optimize( it1 );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		it2 = std::find_if( a.cbegin( ), a.cend( ), pred );
		daw::do_not_optimize( it2 );
	} );
	BOOST_REQUIRE_MESSAGE( it1 == it2, "Wrong return value" );

	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "find_if" );
}

BOOST_AUTO_TEST_CASE( find_if_int64_t ) {
	std::cout << "find_if tests - int64_t\n";
	find_if_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		find_if_test<int64_t>( n );
	}
}
