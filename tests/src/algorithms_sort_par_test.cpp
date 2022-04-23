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
#include <chrono>
#include <cstdint>
#include <iostream>

template<typename Iterator>
void test_sort( Iterator first, Iterator last, daw::string_view label ) {
	if( first == last ) {
		return;
	}
	auto it = first;
	auto last_val = *it;
	++it;
	for( ; it != last; ++it ) {
		if( *it < last_val ) {
			auto const pos = std::distance( first, it );
			std::cerr << "Sequence '" << label << "' not sorted at position ("
			          << std::distance( first, it ) << '/' << std::distance( first, last ) << ")\n";

			auto start = pos > 10 ? std::next( first, pos - 10 ) : first;
			auto const end = std::distance( it, last ) > 10 ? std::next( it, 10 ) : last;
			if( std::distance( start, end ) > 0 ) {
				std::cerr << '[' << *start;
				++start;
				for( ; start != end; ++start ) {
					std::cerr << ", " << *start;
				}
				std::cerr << " ]\n";
			}
			break;
		}
		last_val = *it;
	}
}

template<std::size_t SZ>
void parallel_sort_test( benchmark::State &state ) {
	auto ts = daw::get_task_scheduler( );
	auto a = daw::make_random_data<int64_t>( SZ );
	auto b = std::vector<int64_t>( );
	for( auto _ : state ) {
		state.PauseTiming( );
		{
			benchmark::DoNotOptimize( a );
			b = a;
		}
		state.ResumeTiming( );
		daw::algorithm::parallel::sort( std::data( b ), daw::data_end( b ), ts );
		benchmark::DoNotOptimize( b );
	}
	test_sort( std::data( b ), daw::data_end( b ), "parallel sort test" );
}

BENCHMARK_TEMPLATE( parallel_sort_test, 1'024 );
BENCHMARK_TEMPLATE( parallel_sort_test, 4'096 );
BENCHMARK_TEMPLATE( parallel_sort_test, 16'384 );
BENCHMARK_TEMPLATE( parallel_sort_test, 65'536 );
