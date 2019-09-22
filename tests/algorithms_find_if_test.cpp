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

template<size_t Count>
void find_if_test( size_t SZ ) {
	auto ts = ::daw::get_task_scheduler( );
	ts.start( );
	assert( SZ <= LARGE_TEST_SZ );
	auto const a = [SZ]( ) {
		  auto result = ::daw::make_random_data<int64_t>( SZ, -50, 50 );
		  result.back( ) = 100;
		  return result;
	  }( );

	constexpr auto const pred = []( auto const &value ) noexcept {
		return value == 100;
	};

	auto const par_test = [&]( auto const &ary ) {
		auto it =
		  daw::algorithm::parallel::find_if( ary.cbegin( ), ary.cend( ), pred, ts );
		daw::do_not_optimize( it );
		return it;
	};

#ifdef HAS_PAR_STL
	auto it_std_par = a.end( );
	auto const par_stl_test = [&pred]( auto const &ary ) {
		auto it =
		  std::find_if( std::execution::par, ary.cbegin( ), ary.cend( ), pred );
		daw::do_not_optimize( it );
		return it;
	};
#endif

	auto const ser_test = [&pred]( auto const &ary ) {
		auto it = std::find_if( ary.cbegin( ), ary.cend( ), pred );
		daw::do_not_optimize( it );
		return it;
	};

	auto const vld = []( auto const &v ) {
		if( not v ) {
			return false;
		}
		return *(*v) == 100;
	};

	std::cout << ::daw::utility::to_bytes_per_second( SZ ) + " of int64_t's\n";
	auto const tseq = ::daw::bench_n_test_mbs2<Count, ','>(
	  "  serial", sizeof( int64_t ) * SZ, vld, ser_test, a );
#ifdef HAS_PAR_STL
	auto const tpstl = ::daw::bench_n_test_mbs2<Count, ','>(
	  " par stl", sizeof( int64_t ) * SZ, vld, par_stl_test, a );
#endif
	auto const tpar = ::daw::bench_n_test_mbs2<Count, ','>(
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
	std::cout << "find_if tests - int64_t - "
	          << ::std::thread::hardware_concurrency( ) << " threads\n";
	for( size_t n = 10240; n <= MAX_ITEMS * 2; n *= 4 ) {
		find_if_test<30>( n );
		std::cout << '\n';
	}
	find_if_test<30>( LARGE_TEST_SZ );
}
