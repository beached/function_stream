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

void sort_test( size_t SZ ) {
	auto ts = ::daw::get_task_scheduler( );
	ts.start( );
	// daw::get_task_scheduler( );
	auto a = daw::make_random_data<int64_t>( SZ );

	auto const par_test = [&ts]( auto & ary ) {
		daw::algorithm::parallel::sort(
		  ary.data( ), ary.data( ) + static_cast<ptrdiff_t>( ary.size( ) ), ts );
		daw::do_not_optimize( ary );
	};

#ifdef HAS_PAR_STL
	auto const par_stl_test = [&]( ) {
		std::sort( std::execution::par, a.data( ),
		           a.data( ) + static_cast<ptrdiff_t>( a.size( ) ) );
		daw::do_not_optimize( a );
	};
#endif

	auto const ser_test = []( auto & ary ) {
		std::sort( ary.begin( ), ary.end( ) );
		daw::do_not_optimize( ary );
	};

	std::string const par_title =
	  "Parallel " + ::daw::utility::to_bytes_per_second( SZ ) + " of int64_t's";
	std::string const ser_title =
	  "Serial   " + ::daw::utility::to_bytes_per_second( SZ ) + " of int64_t's";
	auto const tpar = ::daw::bench_n_test_mbs2<5>( par_title, sizeof( int64_t ) * SZ, par_test, a );
	auto const tseq = ::daw::bench_n_test_mbs2<5>( ser_title, sizeof( int64_t ) * SZ, ser_test, a );
	std::cout << "Serial:Parallel perf " << std::setprecision( 1 ) << std::fixed << (tseq/tpar) << '\n';
}

extern char const *const GIT_VERSION;
char const *const GIT_VERSION = SOURCE_CONTROL_REVISION;

int main( ) {
#ifdef DEBUG
	std::cout << "Debug build\n";
	std::cout << GIT_VERSION << '\n';
#endif
	std::cout << "sort tests - int64_t - "
	          << ::std::thread::hardware_concurrency( ) << " threads\n";
	for( size_t n = 1024; n < MAX_ITEMS * 2; n *= 2 ) {
		sort_test( n );
		std::cout << '\n';
	}
	sort_test( LARGE_TEST_SZ );
}
