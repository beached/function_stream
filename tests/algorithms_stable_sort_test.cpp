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

#if defined( _MSC_VER ) and defined( __cpp_lib_parallel_algorithm )
#ifndef HAS_PAR_STL
#define HAS_PAR_STL
#endif
#endif
#if not defined( __cpp_lib_parallel_algorithm )
#undef HAS_PAR_STL
#endif

#ifdef HAS_PAR_STL
#include <execution>
#endif

#include <daw/daw_benchmark.h>
#include <daw/daw_random.h>
#include <daw/daw_string_view.h>

#include "daw/fs/algorithms.h"

#include "common.h"

std::vector<int64_t> const &get_rnd_array( ) {
	static auto const rnd_array = daw::make_random_data<int64_t>( LARGE_TEST_SZ );
	return rnd_array;
}

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
	assert( SZ <= LARGE_TEST_SZ );
	auto const a = std::vector<int64_t>(
	  get_rnd_array( ).begin( ),
	  std::next( get_rnd_array( ).begin( ), static_cast<ptrdiff_t>( SZ ) ) );

	auto const par_test = [&ts]( auto &ary ) {
		daw::algorithm::parallel::stable_sort(
		  ary.data( ), ary.data( ) + static_cast<ptrdiff_t>( ary.size( ) ), ts );
		daw::do_not_optimize( ary );
		return &ary;
	};

#ifdef HAS_PAR_STL
	auto const par_stl_test = []( auto &ary ) {
		std::stable_sort( std::execution::par, ary.data( ),
		                  ary.data( ) + static_cast<ptrdiff_t>( ary.size( ) ) );
		daw::do_not_optimize( ary );
		return &ary;
	};
#endif

	auto const ser_test = []( auto &ary ) {
		std::stable_sort( ary.begin( ), ary.end( ) );
		daw::do_not_optimize( ary );
		return &ary;
	};
	static_assert( std::is_const_v<decltype( a )> );
	auto const vld = []( auto const &v ) {
		auto tmp = v.get( );
		return std::is_sorted( tmp->begin( ), tmp->end( ) );
	};
	std::cout << ::daw::utility::to_bytes_per_second( SZ ) + " of int64_t's\n";
	auto const tseq = ::daw::bench_n_test_mbs2<5, ','>(
	  "  serial", sizeof( int64_t ) * SZ, vld, ser_test, a );
#ifdef HAS_PAR_STL
	auto const tpstl = ::daw::bench_n_test_mbs2<5, ','>(
	  " par stl", sizeof( int64_t ) * SZ, vld, par_stl_test, a );
#endif
	auto const tpar = ::daw::bench_n_test_mbs2<5, ','>(
	  "parallel", sizeof( int64_t ) * SZ, vld, par_test, a );
	std::cout << "Serial:Parallel perf " << std::setprecision( 1 ) << std::fixed
	          << ( tseq / tpar ) << '\n';
#ifdef HAS_PAR_STL
	std::cout << "Serial:ParStl perf " << std::setprecision( 1 ) << std::fixed
	          << ( tseq / tpstl ) << '\n';
	std::cout << "ParStl:Parallel perf " << std::setprecision( 1 ) << std::fixed
	          << ( tpstl / tpar ) << '\n';
#endif
}

extern char const *const GIT_VERSION;
char const *const GIT_VERSION = SOURCE_CONTROL_REVISION;

int main( ) {
#if not defined( NDEBUG ) or defined( DEBUG )
	std::cout << "Debug build\n";
	std::cout << GIT_VERSION << '\n';
#endif
	std::cout << "sort tests - int64_t - "
	          << ::std::thread::hardware_concurrency( ) << " threads\n";
	for( size_t n = 10240; n <= MAX_ITEMS * 2; n *= 4 ) {
		sort_test( n );
		std::cout << '\n';
	}
	sort_test( LARGE_TEST_SZ );
}
