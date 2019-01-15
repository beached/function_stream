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

template<typename T>
void reduce_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	std::vector<T> a;
	a.resize( SZ );
	std::fill( a.begin( ), a.end( ), 1 );
	auto b = a;
	T accum_result1 = 0;
	T accum_result2 = 0;
	auto const result_1 = daw::benchmark( [&]( ) {
		accum_result1 = daw::algorithm::parallel::reduce( a.begin( ), a.end( ),
		                                                  static_cast<T>( 0 ), ts );
		daw::do_not_optimize( accum_result1 );
	} );
	a = b;
	auto const result_2 = daw::benchmark( [&]( ) {
		accum_result2 =
		  std::accumulate( a.begin( ), a.end( ), static_cast<T>( 0 ) );
		daw::do_not_optimize( accum_result2 );
	} );

	daw::expecting( daw::math::nearly_equal( accum_result1, accum_result2 ) );

	a = b;
	auto const result_3 = daw::benchmark( [&]( ) {
		accum_result1 = daw::algorithm::parallel::reduce( a.begin( ), a.end( ),
		                                                  static_cast<T>( 0 ), ts );
		daw::do_not_optimize( accum_result1 );
	} );
	a = b;
	auto const result_4 = daw::benchmark( [&]( ) {
		accum_result2 =
		  std::accumulate( a.begin( ), a.end( ), static_cast<T>( 0 ) );
		daw::do_not_optimize( accum_result2 );
	} );

	daw::expecting( daw::math::nearly_equal( accum_result1, accum_result2 ) );

	auto const par_min = std::min( result_1, result_3 );
	auto const seq_min = std::min( result_2, result_4 );
	display_info( seq_min, par_min, SZ, sizeof( T ), "reduce" );
}

template<typename value_t, typename BinaryOp>
void reduce_test2( size_t SZ, value_t init, BinaryOp bin_op ) {
	auto ts = daw::get_task_scheduler( );
	auto a = daw::make_random_data<value_t>( SZ );
	auto b = a;
	value_t accum_result1 = 0;
	value_t accum_result2 = 0;

	auto const result_1 = daw::benchmark( [&]( ) {
		accum_result1 = daw::algorithm::parallel::reduce<value_t>(
		  a.begin( ), a.end( ), init, bin_op, ts );
		daw::do_not_optimize( accum_result1 );
	} );
	a = b;
	auto const result_2 = daw::benchmark( [&]( ) {
		accum_result2 = std::accumulate( a.begin( ), a.end( ), init, bin_op );
		daw::do_not_optimize( accum_result2 );
	} );
	daw::expecting( accum_result1, accum_result2 );
	a = b;
	auto const result_3 = daw::benchmark( [&]( ) {
		accum_result1 = daw::algorithm::parallel::reduce<value_t>(
		  a.begin( ), a.end( ), init, bin_op, ts );
		daw::do_not_optimize( accum_result1 );
	} );
	a = b;
	auto const result_4 = daw::benchmark( [&]( ) {
		accum_result2 = std::accumulate( a.begin( ), a.end( ), init, bin_op );
		daw::do_not_optimize( accum_result2 );
	} );
	daw::expecting( accum_result1, accum_result2 );
	auto const par_min = std::min( result_1, result_3 );
	auto const seq_min = std::min( result_2, result_4 );
	display_info( seq_min, par_min, SZ, sizeof( value_t ), "reduce2" );
}

void reduce_double( ) {
	std::cout << "reduce tests - double\n";
	reduce_test<double>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		reduce_test<double>( n );
	}
}

void reduce_int64_t( ) {
	std::cout << "reduce tests - int64_t\n";
	reduce_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		reduce_test<int64_t>( n );
	}
}

void reduce2_int64_t( ) {
	std::cout << "reduce 2 tests - uint64_t\n";
	auto const bin_op = []( auto const &lhs, auto const &rhs ) noexcept {
		return lhs * rhs;
	};
	reduce_test2<uint64_t>( LARGE_TEST_SZ, 1, bin_op );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		reduce_test2<uint64_t>( n, 1, bin_op );
	}
}

void reduce3_double( ) {
	std::cout << "reduce 3 tests - double\n";
	reduce_test<double>( LARGE_TEST_SZ * 10 );
	reduce_test<double>( 6'000'000 );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		reduce_test<double>( n );
	}
}

int main( ) {
	reduce_double( );
	reduce_int64_t( );
	reduce2_int64_t( );
	reduce3_double( );
}
