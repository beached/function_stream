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
#include <iostream>

#ifdef _WIN32
#define HAS_PAR_STL
#include <execution>
#endif

#include <daw/daw_benchmark.h>
#include <daw/daw_random.h>
#include <daw/daw_string_view.h>

#include "daw/fs/algorithms.h"

#include "common.h"

template<typename Iterator>
void test_sort( Iterator const first, Iterator const last,
                daw::string_view label ) {
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
			          << std::distance( first, it ) << '/'
			          << std::distance( first, last ) << ")\n";

			auto start = pos > 10 ? std::next( first, pos - 10 ) : first;
			auto const end =
			  std::distance( it, last ) > 10 ? std::next( it, 10 ) : last;
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

void sort_test( size_t SZ, unsigned ThreadCount ) {
	auto ts = daw::task_scheduler( ThreadCount, true );
	ts.start( );
	// daw::get_task_scheduler( );
	auto a = daw::make_random_data<int64_t>( SZ );

	auto b = a;

	auto const par_test = [&]( ) {
		daw::algorithm::parallel::sort(
		  a.data( ), a.data( ) + static_cast<ptrdiff_t>( a.size( ) ), ts );
		daw::do_not_optimize( a );
	};

#ifdef HAS_PAR_STL
	auto const par_stl_test = [&]( ) {
		std::sort( std::execution::par, a.data( ),
		           a.data( ) + static_cast<ptrdiff_t>( a.size( ) ) );
		daw::do_not_optimize( a );
	};
#endif

	auto const ser_test = [&]( ) {
		std::sort( a.begin( ), a.end( ) );
		daw::do_not_optimize( a );
	};

	auto const par_result_1 = daw::benchmark( par_test );
	test_sort( a.begin( ), a.end( ), "p_result_1" );
	a = b;

	auto const ser_result_1 = daw::benchmark( ser_test );
	test_sort( a.begin( ), a.end( ), "s_result_1" );
	a = b;
	auto const par_result_2 = daw::benchmark( par_test );
	test_sort( a.begin( ), a.end( ), "p_result2" );
	a = b;

	auto const ser_result_2 = daw::benchmark( ser_test );
	test_sort( a.begin( ), a.end( ), "s_result2" );

	auto const par_min = std::min( par_result_1, par_result_2 );
	auto const seq_min = std::min( ser_result_1, ser_result_2 );

	display_info( seq_min, par_min, SZ, sizeof( int64_t ), "sort_merge" );
}

int main( int argc, char const **argv ) {
#ifdef DEBUG
	std::cout << "Debug build\n";
#endif
	if( argc > 1 and std::string( argv[1] ) == "full" ) {
		for( unsigned t = 2; t <= std::thread::hardware_concurrency( ) * 2U; ++t ) {
			std::cout << "sort_merge tests - int64_t - " << t << " threads\n";
			for( size_t n = 1024; n < MAX_ITEMS * 2; n *= 2 ) {
				sort_test( n, t );
				std::cout << '\n';
			}
			sort_test( LARGE_TEST_SZ, t );
		}
		return 0;
	}
	auto const t = std::thread::hardware_concurrency( );
	std::cout << "sort_merge tests - int64_t - " << t << " threads\n";
	for( size_t n = 1024; n < MAX_ITEMS * 2; n *= 2 ) {
		sort_test( n, t );
		std::cout << '\n';
	}
	sort_test( LARGE_TEST_SZ, t );
}
