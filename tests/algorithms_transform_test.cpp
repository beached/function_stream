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
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include <daw/daw_benchmark.h>
#include <daw/daw_math.h>
#include <daw/daw_random.h>
#include <daw/daw_string_view.h>
#include <daw/daw_utility.h>

#define BOOST_TEST_MODULE parallel_algorithms_transform
#include <daw/boost_test.h>

#include "algorithms.h"

#include "common.h"

template<typename value_t>
void transform_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	auto a = daw::make_random_data<value_t>( SZ );
	std::vector<value_t> b;
	std::vector<value_t> c;
	b.resize( SZ );
	c.resize( SZ );

	auto unary_op = []( auto const &value ) { return value + value; };

	auto const result_1 = daw::benchmark( [&]( ) {
		daw::algorithm::parallel::transform( a.cbegin( ), a.cend( ), b.begin( ),
		                                     unary_op, ts );
		daw::do_not_optimize( b );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		std::transform( a.cbegin( ), a.cend( ), c.begin( ), unary_op );
		daw::do_not_optimize( c );
	} );
	BOOST_REQUIRE_MESSAGE(
	  std::equal( b.cbegin( ), b.cend( ), c.cbegin( ), c.cend( ) ),
	  "Wrong return value" );
	auto const result_3 = daw::benchmark( [&]( ) {
		daw::algorithm::parallel::transform( a.cbegin( ), a.cend( ), b.begin( ),
		                                     unary_op, ts );
		daw::do_not_optimize( b );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		std::transform( a.cbegin( ), a.cend( ), c.begin( ), unary_op );
		daw::do_not_optimize( c );
	} );
	BOOST_REQUIRE_MESSAGE(
	  std::equal( b.cbegin( ), b.cend( ), c.cbegin( ), c.cend( ) ),
	  "Wrong return value" );
	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "transform" );
}

template<typename value_t>
void transform_test2( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	auto a = daw::make_random_data<value_t>( SZ, -10, 10 );
	std::vector<value_t> b{};
	b.resize( SZ );
	std::vector<value_t> c{};
	c.resize( SZ );

	auto unary_op = []( auto const &value ) { return value * value; };

	auto const result_1 = daw::benchmark( [&]( ) {
		daw::algorithm::parallel::transform( a.cbegin( ), a.cend( ), b.begin( ),
		                                     unary_op, ts );
		daw::do_not_optimize( b );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		std::transform( a.cbegin( ), a.cend( ), c.begin( ), unary_op );
		daw::do_not_optimize( c );
	} );
	BOOST_REQUIRE_MESSAGE(
	  std::equal( b.cbegin( ), b.cend( ), c.cbegin( ), c.cend( ) ),
	  "Wrong return value" );

	auto const result_3 = daw::benchmark( [&]( ) {
		daw::algorithm::parallel::transform( a.cbegin( ), a.cend( ), b.begin( ),
		                                     unary_op, ts );
		daw::do_not_optimize( b );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		std::transform( a.cbegin( ), a.cend( ), c.begin( ), unary_op );
		daw::do_not_optimize( c );
	} );
	BOOST_REQUIRE_MESSAGE(
	  std::equal( b.cbegin( ), b.cend( ), c.cbegin( ), c.cend( ) ),
	  "Wrong return value" );

	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "transform" );
}

BOOST_AUTO_TEST_CASE( transform_int64_t ) {
	std::cout << "transform tests - int64_t\n";
	transform_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		transform_test<int64_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( transform2_int64_t ) {
	std::cout << "transform2 tests - int64_t\n";
	transform_test2<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		transform_test2<int64_t>( n );
	}
}
