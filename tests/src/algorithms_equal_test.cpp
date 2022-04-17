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

template<size_t Count>
void equal_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	ts.start( );
	assert( SZ <= LARGE_TEST_SZ );

	alignas( 128 ) auto const a = [SZ]( ) {
		alignas( 128 ) auto result =
		  daw::make_random_data<int64_t>( SZ, -50, 50 );
		result.back( ) = 100;
		return result;
	}( );

	alignas( 128 ) auto const b = [&a]( ) {
		auto result = a;
		daw::do_not_optimize( result );
		return result;
	}( );

	auto const par_test = [&]( auto const &ary0, auto const &ary1 ) {
		auto result = daw::algorithm::parallel::equal( ary0.begin( ), ary0.end( ),
		                                                 ary1.begin( ), ary1.end( ),
		                                                 std::equal_to{}, ts );
		daw::do_not_optimize( result );
		return result;
	};

#ifdef HAS_PAR_STL
	auto const par_stl_test = []( auto const &ary0, auto const &ary1 ) {
		auto result =
		  std::equal( std::execution::par, ary0.begin( ), ary0.end( ),
		                ary1.begin( ), ary1.end( ), std::equal_to{} );
		daw::do_not_optimize( result );
		return result;
	};
#endif

	auto const ser_test = []( auto const &ary0, auto const &ary1 ) {
		auto result = std::equal( ary0.begin( ), ary0.end( ), ary1.begin( ),
		                            ary1.end( ), std::equal_to{} );
		daw::do_not_optimize( result );
		return result;
	};

	auto const vld = []( auto const &v ) {
		if( not v ) {
			return false;
		}
		return *v;
	};

	std::cout << daw::utility::to_bytes_per_second( SZ ) + " of int64_t's\n";
	auto const tseq = daw::bench_n_test_mbs2<Count, ','>(
	  "  serial", sizeof( int64_t ) * SZ, vld, ser_test, a, b );
	auto const tseq_min = *std::min_element( tseq.begin( ), tseq.end( ) );
	show_times( tseq );
#ifdef HAS_PAR_STL
	auto const tpstl = daw::bench_n_test_mbs2<Count, ','>(
	  " par stl", sizeof( int64_t ) * SZ, vld, par_stl_test, a, b );
	auto const tpstl_min = *std::min_element( tseq.begin( ), tseq.end( ) );
	show_times( tpstl );
#endif
	auto const tpar = daw::bench_n_test_mbs2<Count, ','>(
	  "parallel", sizeof( int64_t ) * SZ, vld, par_test, a, b );
	auto const tpar_min = *std::min_element( tseq.begin( ), tseq.end( ) );
	show_times( tpar );
	std::cout << "Serial:Parallel perf " << std::setprecision( 1 ) << std::fixed
	          << ( tseq_min / tpar_min ) << '\n';
#ifdef HAS_PAR_STL
	std::cout << "Serial:ParStl perf " << std::setprecision( 1 ) << std::fixed
	          << ( tseq_min / tpstl_min ) << '\n';
	std::cout << "ParStl:Parallel perf " << std::setprecision( 1 ) << std::fixed
	          << ( tpstl_min / tpar_min ) << '\n';
#endif
}

template<size_t Count>
void equal_test_str( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	ts.start( );
	assert( SZ <= LARGE_TEST_SZ );

	alignas( 128 ) auto const a = [SZ]( ) {
		auto result = daw::make_random_data<char, std::string>( SZ, 'a', 'z' );
		daw::do_not_optimize( result );
		return result;
	}( );

	alignas( 128 ) auto const b = [&a]( ) {
		auto result = a;
		daw::do_not_optimize( result );
		return result;
	}( );

	auto const par_test = [&]( auto const &ary0, auto const &ary1 ) {
		auto result = daw::algorithm::parallel::equal( ary0.begin( ), ary0.end( ),
		                                                 ary1.begin( ), ary1.end( ),
		                                                 std::equal_to{}, ts );
		daw::do_not_optimize( result );
		return result;
	};

#ifdef HAS_PAR_STL
	auto const par_stl_test = []( auto const &ary0, auto const &ary1 ) {
		auto result =
		  std::equal( std::execution::par, ary0.begin( ), ary0.end( ),
		                ary1.begin( ), ary1.end( ), std::equal_to{} );
		daw::do_not_optimize( result );
		return result;
	};
#endif

	auto const ser_test = []( auto const &ary0, auto const &ary1 ) {
		auto result = std::equal( ary0.begin( ), ary0.end( ), ary1.begin( ),
		                            ary1.end( ), std::equal_to{} );
		daw::do_not_optimize( result );
		return result;
	};

	auto const vld = []( auto const &v ) {
		if( not v ) {
			return false;
		}
		return *v;
	};

	std::cout << daw::utility::to_bytes_per_second( SZ ) + " std::string\n";
	auto const tseq = daw::bench_n_test_mbs2<Count, ','>(
	  "  serial", sizeof( int64_t ) * SZ, vld, ser_test, a, b );
	auto const tseq_min = *std::min_element( tseq.begin( ), tseq.end( ) );
	show_times( tseq );
#ifdef HAS_PAR_STL
	auto const tpstl = daw::bench_n_test_mbs2<Count, ','>(
	  " par stl", sizeof( int64_t ) * SZ, vld, par_stl_test, a, b );
	auto const tpstl_min = *std::min_element( tpstl.begin( ), tpstl.end( ) );
	show_times( tpstl );
#endif
	auto const tpar = daw::bench_n_test_mbs2<Count, ','>(
	  "parallel", sizeof( int64_t ) * SZ, vld, par_test, a, b );
	auto const tpar_min = *std::min_element( tpar.begin( ), tpar.end( ) );
	show_times( tpar );
	std::cout << "Serial:Parallel perf " << std::setprecision( 1 ) << std::fixed
	          << ( tseq_min / tpar_min ) << '\n';
#ifdef HAS_PAR_STL
	std::cout << "Serial:ParStl perf " << std::setprecision( 1 ) << std::fixed
	          << ( tseq_min / tpstl_min ) << '\n';
	std::cout << "ParStl:Parallel perf " << std::setprecision( 1 ) << std::fixed
	          << ( tpstl_min / tpar_min ) << '\n';
#endif
}

extern char const *const GIT_VERSION;
char const *const GIT_VERSION = SOURCE_CONTROL_REVISION;

int main( ) {
#if not defined( NDEBUG ) or defined( DEBUG )
	std::cout << "Debug build\n";
	std::cout << GIT_VERSION << '\n';
#endif

	std::cout << "equal tests - int64_t - "
	          << std::thread::hardware_concurrency( ) << " threads\n";
	for( size_t n = 10240; n <= MAX_ITEMS * 4; n *= 4 ) {
		equal_test<30>( n );
		std::cout << '\n';
	}
	equal_test<10>( LARGE_TEST_SZ );

	std::cout << "equal tests - string - "
	          << std::thread::hardware_concurrency( ) << " threads\n";
	for( size_t n = 10240; n <= MAX_ITEMS * 4; n *= 4 ) {
		equal_test_str<30>( n );
		std::cout << '\n';
	}
	equal_test_str<10>( LARGE_TEST_SZ );
}
