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

#define BOOST_TEST_MODULE parallel_algorithms_equal
#include <daw/boost_test.h>

#include "daw/fs/algorithms.h"

#include "common.h"

template<typename value_t>
void equal_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	std::vector<value_t> a;
	a.resize( SZ );
	for( size_t n = 0; n < a.size( ); ++n ) {
		a[n] = static_cast<value_t>( n );
	}
	auto b = a;

	auto const pred = []( value_t const &lhs, value_t const &rhs ) noexcept {
		bool result = lhs == rhs;
		return result;
	};

	bool b1 = false;
	bool b2 = false;
	auto const result_1 = daw::benchmark( [&]( ) {
		b1 = daw::algorithm::parallel::equal( a.cbegin( ), a.cend( ), b.cbegin( ),
		                                      b.cend( ), pred, ts );
		daw::do_not_optimize( b1 );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		b2 = std::equal( a.cbegin( ), a.cend( ), b.cbegin( ), b.cend( ), pred );
		daw::do_not_optimize( b2 );
	} );
	BOOST_REQUIRE_MESSAGE( b1 == b2, "Wrong return value" );

	a.back( ) = 0;
	b1 = false;
	b2 = false;
	auto const result_3 = daw::benchmark( [&]( ) {
		b1 = daw::algorithm::parallel::equal( a.cbegin( ), a.cend( ), b.cbegin( ),
		                                      b.cend( ), pred, ts );
		daw::do_not_optimize( b1 );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		b2 = std::equal( a.cbegin( ), a.cend( ), b.cbegin( ), b.cend( ), pred );
		daw::do_not_optimize( b2 );
	} );
	BOOST_REQUIRE_MESSAGE( b1 == b2, "Wrong return value" );

	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "equal" );
}

void equal_test_str( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	std::vector<std::string> a;
	a.reserve( SZ );
	std::string const blah = "AAAAAAAA";
	std::fill_n( std::back_inserter( a ), SZ, blah );
	std::vector<std::string> b;
	b.reserve( SZ );
	std::copy( a.cbegin( ), a.cend( ), std::back_inserter( b ) );

	auto const pred = []( auto const &lhs, auto const &rhs ) noexcept {
		auto const result = lhs == rhs;
		if( result ) {
			return true;
		}
		return false;
	};

	bool b1 = false;
	bool b2 = false;
	auto const result_1 = daw::benchmark( [&]( ) {
		b1 = daw::algorithm::parallel::equal( a.cbegin( ), a.cend( ), b.cbegin( ),
		                                      b.cend( ), pred, ts );
		daw::do_not_optimize( b1 );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		b2 = std::equal( a.cbegin( ), a.cend( ), b.cbegin( ), b.cend( ), pred );
		daw::do_not_optimize( b2 );
	} );
	BOOST_REQUIRE_MESSAGE( b1 == b2, "Wrong return value" );

	a[3 * ( a.size( ) / 4 ) + 1] =
	  std::string{"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"};
	b1 = false;
	b2 = false;
	auto const result_3 = daw::benchmark( [&]( ) {
		b1 = daw::algorithm::parallel::equal( a.cbegin( ), a.cend( ), b.cbegin( ),
		                                      b.cend( ), pred, ts );
		daw::do_not_optimize( b1 );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		b2 = std::equal( a.cbegin( ), a.cend( ), b.cbegin( ), b.cend( ), pred );
		daw::do_not_optimize( b2 );
	} );
	BOOST_REQUIRE_MESSAGE( b1 == b2, "Wrong return value" );

	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, blah.size( ), "equal" );
}

BOOST_AUTO_TEST_CASE( equal_int64_t ) {
	std::cout << "equal tests - int64_t\n";
	equal_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		equal_test<int64_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( equal_string ) {
	std::cout << "equal tests - std::string\n";
	equal_test_str( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		equal_test_str( n );
	}
}
