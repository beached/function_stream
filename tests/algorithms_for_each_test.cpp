// The MIT License (MIT)
//
// Copyright (c) 2017-2018 Darrell Wright
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

#define BOOST_TEST_MODULE parallel_algorithms_for_each
#include <daw/boost_test.h>

#include "algorithms.h"

BOOST_AUTO_TEST_CASE( start_task_scheduler ) {
	// Prime task scheduler so we don't pay to start it up in first test
	auto ts = daw::get_task_scheduler( );
	BOOST_REQUIRE( ts.started( ) );
	daw::do_not_optimize( ts );
}

namespace {
	template<typename T>
	double calc_speedup( T seq_time, T par_time ) {
		static double const N = std::thread::hardware_concurrency( );
		return 100.0 * ( ( seq_time / N ) / par_time );
	}

	void display_info( double seq_time, double par_time, double count,
	                   size_t bytes, daw::string_view label ) {
		using namespace std::chrono;
		using namespace date;

		auto const make_seconds = []( double t, double c ) {
			auto val = ( t / c ) * 1000000000000.0;

			if( val < 1000 ) {
				return std::to_string( static_cast<uint64_t>( val ) ) + "ps";
			}
			val /= 1000.0;
			if( val < 1000 ) {
				return std::to_string( static_cast<uint64_t>( val ) ) + "ns";
			}
			val /= 1000.0;
			if( val < 1000 ) {
				return std::to_string( static_cast<uint64_t>( val ) ) + "µs";
			}
			val /= 1000.0;
			if( val < 1000 ) {
				return std::to_string( static_cast<uint64_t>( val ) ) + "ms";
			}
			val /= 1000.0;
			return std::to_string( static_cast<uint64_t>( val ) ) + "s";
		};

		auto const mbs = [count, bytes]( double t ) {
			using result_t = double;
			std::stringstream ss;
			ss << std::setprecision( 1 ) << std::fixed;
			auto val = ( count * static_cast<double>( bytes ) ) / t;
			if( val < 1024 ) {
				ss << ( static_cast<result_t>( val * 100.0 ) / 100 ) << "bytes";
				return ss.str( );
			}
			val /= 1024.0;
			if( val < 1024 ) {
				ss << ( static_cast<result_t>( val * 100.0 ) / 100 ) << "KB";
				return ss.str( );
			}
			val /= 1024.0;
			if( val < 1024 ) {
				ss << ( static_cast<result_t>( val * 100.0 ) / 100 ) << "MB";
				return ss.str( );
			}
			val /= 1024.0;
			ss << ( static_cast<result_t>( val * 100.0 ) / 100 ) << "GB";
			return ss.str( );
		};

		std::cout << std::setprecision( 1 ) << std::fixed << label << ": size->"
		          << static_cast<uint64_t>( count ) << " " << mbs( 1 ) << " %max->"
		          << calc_speedup( seq_time, par_time ) << "("
		          << ( seq_time / par_time ) << "/"
		          << std::thread::hardware_concurrency( ) << "X) par_total->"
		          << make_seconds( par_time, 1 ) << " par_item->"
		          << make_seconds( par_time, count ) << " throughput->"
		          << mbs( par_time ) << "/s seq_total->"
		          << make_seconds( seq_time, 1 ) << " seq_item->"
		          << make_seconds( seq_time, count ) << " throughput->"
		          << mbs( seq_time ) << "/s \n";
	}

	template<typename T>
	void for_each_test( size_t SZ ) {
		auto ts = daw::get_task_scheduler( );
		bool found = false;
		std::vector<T> a;
		a.resize( SZ );
		std::fill( a.begin( ), a.end( ), 1 );
		a[SZ / 2] = 4;
		auto const find_even = [&]( T const &x ) {
			if( static_cast<intmax_t>( x ) % 2 == 0 ) {
				found = true;
			}
			daw::do_not_optimize( found );
		};
		auto const result_1 = daw::benchmark( [&]( ) {
			daw::algorithm::parallel::for_each( a.cbegin( ), a.cend( ), find_even,
			                                    ts );
		} );
		auto const result_2 = daw::benchmark( [&]( ) {
			for( auto const &item : a ) {
				find_even( item );
			}
		} );
		auto const result_3 = daw::benchmark( [&]( ) {
			daw::algorithm::parallel::for_each( a.cbegin( ), a.cend( ), find_even,
			                                    ts );
		} );
		auto const result_4 = daw::benchmark( [&]( ) {
			for( auto const &item : a ) {
				find_even( item );
			}
		} );
		auto const par_min = ( result_1 + result_3 ) / 2;
		auto const seq_min = ( result_2 + result_4 ) / 2;
		display_info( seq_min, par_min, SZ, sizeof( T ), "for_each" );
	}

	// static size_t const MAX_ITEMS = 134'217'728;
	// static size_t const LARGE_TEST_SZ = 268'435'456;

	size_t const MAX_ITEMS = 14'217'728;
	size_t const LARGE_TEST_SZ = 28'435'456;

	// static size_t const MAX_ITEMS = 4'217'728;
	// static size_t const LARGE_TEST_SZ = 2 * MAX_ITEMS;
} // namespace

BOOST_AUTO_TEST_CASE( for_each_double ) {
	std::cout << "for_each tests - double\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		for_each_test<double>( n );
	}
}

BOOST_AUTO_TEST_CASE( for_each_int64_t ) {
	std::cout << "for_each tests - int64_t\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		for_each_test<int64_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( for_each_int32_t ) {
	std::cout << "for_each tests - int32_t\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		for_each_test<int32_t>( n );
	}
}

