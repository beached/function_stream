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

template<std::ptrdiff_t SZ>
std::tuple<std::vector<int64_t>, std::vector<int64_t>> random_get( ) {
	static auto const a = [] {
		auto result = daw::make_random_data<int64_t>( static_cast<std::size_t>( SZ ), -50, 50 );
		result.back( ) = 100;
		return result;
	}( );

	static auto const b = [] {
		auto result = a;
		benchmark::DoNotOptimize( result );
		return result;
	}( );

	return { a, b };
}

template<std::ptrdiff_t SZ>
static void bench_daw_par_equal( benchmark::State &s ) {
	auto const ary = random_get<SZ>( );
	auto ts = daw::get_task_scheduler( );
	ts.start( );
	for( auto _ : s ) {
		[&, &a = std::get<0>( ary ), &b = std::get<1>( ary ) ]( ) {
			benchmark::DoNotOptimize( a );
			benchmark::DoNotOptimize( b );
			auto result = daw::algorithm::parallel::equal( a.begin( ),
			                                               a.end( ),
			                                               b.begin( ),
			                                               b.end( ),
			                                               std::equal_to<>{ },
			                                               ts );
			benchmark::DoNotOptimize( result );
			benchmark::ClobberMemory( );
		}
		( );
	}
}
BENCHMARK_TEMPLATE( bench_daw_par_equal, 1'024 );
BENCHMARK_TEMPLATE( bench_daw_par_equal, 4'096 );
BENCHMARK_TEMPLATE( bench_daw_par_equal, 16'384 );
BENCHMARK_TEMPLATE( bench_daw_par_equal, 65'536 );
BENCHMARK_TEMPLATE( bench_daw_par_equal, 2'097'152 );
/*
#if defined( HAS_PAR_STL )
template<std::ptrdiff_t SZ>
static void bench_par_stl_equal( benchmark::State &s ) {
  auto const ary = random_get<SZ>( );

  for( auto _ : s ) {
    [&a = std::get<0>( ary ), &b = std::get<1>( ary ) ]( ) {
      benchmark::DoNotOptimize( a );
      benchmark::DoNotOptimize( b );
      std::equal( std::execution::par,
                  a.begin( ),
                  a.end( ),
                  b.begin( ),
                  b.end( ),
                  std::equal_to<>{ } );
      benchmark::DoNotOptimize( result );
      benchmark::ClobberMemory( );
    }
    ( );
  }
}
BENCHMARK_TEMPLATE( bench_par_stl_equal, 1'024 );
BENCHMARK_TEMPLATE( bench_par_stl_equal, 4'096 );
BENCHMARK_TEMPLATE( bench_par_stl_equal, 16'384 );
BENCHMARK_TEMPLATE( bench_par_stl_equal, 65'536 );
BENCHMARK_TEMPLATE( bench_par_stl_equal, 2'097'152 );
#endif
*/
template<std::ptrdiff_t SZ>
static void bench_stl_equal( benchmark::State &s ) {
	auto const ary = random_get<SZ>( );
	for( auto _ : s ) {
		[&a = std::get<0>( ary ), &b = std::get<1>( ary ) ]( ) {
			benchmark::DoNotOptimize( a );
			benchmark::DoNotOptimize( b );
			auto result = std::equal( a.begin( ), a.end( ), b.begin( ), b.end( ), std::equal_to<>{ } );
			benchmark::DoNotOptimize( result );
			benchmark::ClobberMemory( );
		}
		( );
	}
}
BENCHMARK_TEMPLATE( bench_stl_equal, 1'024 );
BENCHMARK_TEMPLATE( bench_stl_equal, 4'096 );
BENCHMARK_TEMPLATE( bench_stl_equal, 16'384 );
BENCHMARK_TEMPLATE( bench_stl_equal, 65'536 );
BENCHMARK_TEMPLATE( bench_stl_equal, 2'097'152 );

