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

#include "daw/fs/algorithms.h"

#include "common.h"

template<typename value_t>
void min_element_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	auto a = daw::make_random_data<value_t>( SZ );
	value_t min_result1 = 0;
	value_t min_result2 = 0;

	auto const result_1 = daw::benchmark( [&]( ) {
		min_result1 =
		  *daw::algorithm::parallel::min_element( a.begin( ), a.end( ), ts );
		daw::do_not_optimize( min_result1 );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		min_result2 = *std::min_element( a.begin( ), a.end( ) );
		daw::do_not_optimize( min_result2 );
	} );
	daw::expecting( min_result1 == min_result2 );
	auto const result_3 = daw::benchmark( [&]( ) {
		min_result1 =
		  *daw::algorithm::parallel::min_element( a.begin( ), a.end( ), ts );
		daw::do_not_optimize( min_result1 );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		min_result2 = *std::min_element( a.begin( ), a.end( ) );
		daw::do_not_optimize( min_result2 );
	} );
	daw::expecting( min_result1 == min_result2 );
	auto const par_min = std::min( result_1, result_3 );
	auto const seq_min = std::min( result_2, result_4 );
	display_info( seq_min, par_min, SZ, sizeof( value_t ), "min_element" );
}

template<typename value_t>
void max_element_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	auto a = daw::make_random_data<value_t>( SZ );
	value_t max_result1 = 0;
	value_t max_result2 = 0;

	auto const result_1 = daw::benchmark( [&]( ) {
		max_result1 =
		  *daw::algorithm::parallel::max_element( a.begin( ), a.end( ), ts );
		daw::do_not_optimize( max_result1 );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		max_result2 = *std::max_element( a.begin( ), a.end( ) );
		daw::do_not_optimize( max_result2 );
	} );
	daw::expecting( max_result1, max_result2 );
	auto const result_3 = daw::benchmark( [&]( ) {
		max_result1 =
		  *daw::algorithm::parallel::max_element( a.begin( ), a.end( ), ts );
		daw::do_not_optimize( max_result1 );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		max_result2 = *std::max_element( a.begin( ), a.end( ) );
		daw::do_not_optimize( max_result2 );
	} );
	daw::expecting( max_result1, max_result2 );
	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "max_element" );
}

void min_element_int64_t( ) {
	std::cout << "min_element tests - int64_t\n";
	min_element_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		min_element_test<int64_t>( n );
	}
}

void max_element_int64_t( ) {
	std::cout << "max_element tests - int64_t\n";
	max_element_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		max_element_test<int64_t>( n );
	}
}

int main( ) {
	min_element_int64_t( );
	max_element_int64_t( );
}
