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

#include "common.h"
#include "daw/fs/algorithms.h"

#include <daw/daw_random.h>
#include <daw/daw_string_view.h>

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <vector>

#if defined( __cpp_lib_parallel_algorithm )
#define HAS_PAR_STL
#include <execution>
#endif

std::vector<int64_t> const &get_rnd_array( ) {
	static auto const rnd_array = daw::make_random_data<int64_t>( LARGE_TEST_SZ );
	return rnd_array;
}

template<std::ptrdiff_t SZ>
static void bench_daw_par_sort( benchmark::State &s ) {
	auto ts = daw::get_task_scheduler( );
	ts.start( );
	auto const &big_vec = get_rnd_array( );
	for( auto _ : s ) {
		[&ts]( std::vector<int64_t> v ) __attribute__( ( noinline ) ) {
			daw::algorithm::parallel::sort( std::data( v ), daw::data_end( v ), ts );
			benchmark::ClobberMemory( );
		}
		( std::vector<int64_t>( std::data( big_vec ), std::data( big_vec ) + SZ ) );
	}
}
BENCHMARK_TEMPLATE( bench_daw_par_sort, 1'024 );
/*
BENCHMARK_TEMPLATE( bench_daw_par_sort, 4'096 );
BENCHMARK_TEMPLATE( bench_daw_par_sort, 16'384 );
BENCHMARK_TEMPLATE( bench_daw_par_sort, 65'536 );
*/
/*
#if defined( HAS_PAR_STL )
template<std::ptrdiff_t SZ>
static void bench_par_stl_sort( benchmark::State &s ) {
	auto ts = daw::get_task_scheduler( );
	ts.start( );
	auto const &big_vec = get_rnd_array( );
	for( auto _ : s ) {
		[]( std::vector<int64_t> v ) {
			std::sort( std::execution::par, std::data( v ), daw::data_end( v ) );
			benchmark::ClobberMemory( );
		}
		( std::vector<int64_t>( std::data( big_vec ), std::data( big_vec ) + SZ ) );
	}
}
BENCHMARK_TEMPLATE( bench_par_stl_sort, 1'024 );
BENCHMARK_TEMPLATE( bench_par_stl_sort, 4'096 );
BENCHMARK_TEMPLATE( bench_par_stl_sort, 16'384 );
BENCHMARK_TEMPLATE( bench_par_stl_sort, 65'536 );
#endif
*/
template<std::ptrdiff_t SZ>
static void bench_stl_sort( benchmark::State &s ) {
	auto ts = daw::get_task_scheduler( );
	ts.start( );
	auto const &big_vec = get_rnd_array( );
	for( auto _ : s ) {
		[]( std::vector<int64_t> v ) __attribute__( ( noinline ) ) {
			std::sort( std::data( v ), daw::data_end( v ) );
			benchmark::ClobberMemory( );
		}
	  ( std::vector<int64_t>( std::data( big_vec ), std::data( big_vec ) + SZ ) );
	}
}
BENCHMARK_TEMPLATE( bench_stl_sort, 1'024 );
/*
BENCHMARK_TEMPLATE( bench_stl_sort, 4'096 );
BENCHMARK_TEMPLATE( bench_stl_sort, 16'384 );
BENCHMARK_TEMPLATE( bench_stl_sort, 65'536 );
*/

