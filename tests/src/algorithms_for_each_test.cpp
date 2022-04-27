// The MIT License (MIT)
//
// Copyright (c) Darrell Wright
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
void for_each_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	std::atomic_bool found = false;
	std::vector<T> a;
	a.resize( SZ );
	std::fill( a.begin( ), a.end( ), 1 );
	a[SZ / 2] = 4;
	auto const find_even = [&]( T const &x ) {
		if( static_cast<std::int64_t>( x ) % 2 == 0 ) {
			found = true;
		}
		daw::do_not_optimize( found );
	};
	auto const result_1 = daw::benchmark(
	  [&]( ) { daw::algorithm::parallel::for_each( a.cbegin( ), a.cend( ), find_even, ts ); } );
	auto const result_2 = daw::benchmark( [&]( ) {
		for( auto const &item : a ) {
			find_even( item );
		}
	} );
	auto const result_3 = daw::benchmark(
	  [&]( ) { daw::algorithm::parallel::for_each( a.cbegin( ), a.cend( ), find_even, ts ); } );
	auto const result_4 = daw::benchmark( [&]( ) {
		for( auto const &item : a ) {
			find_even( item );
		}
	} );
	auto const par_min = ( result_1 + result_3 ) / 2;
	auto const seq_min = ( result_2 + result_4 ) / 2;
	display_info( seq_min, par_min, SZ, sizeof( T ), "for_each" );
}

void for_each_double( ) {
	std::cout << "for_each tests - double\n";
	for( size_t n = 128; n < MAX_ITEMS * 2U; n *= 2U ) {
		for_each_test<double>( n );
	}
}

void for_each_uint64_t( ) {
	std::cout << "for_each tests - int64_t\n";
	for( size_t n = 128; n < MAX_ITEMS * 2U; n *= 2U ) {
		for_each_test<std::uint64_t>( n );
	}
}

void for_each_uint32_t( ) {
	std::cout << "for_each tests - int32_t\n";
	for( size_t n = 128; n < MAX_ITEMS * 2U; n *= 2U ) {
		for_each_test<std::uint32_t>( n );
	}
}

int main( ) {
	for_each_double( );
	for_each_uint64_t( );
	for_each_uint32_t( );
}
