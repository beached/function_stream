// The MIT License (MIT)
//
// Copyright (c) 2016-2018 Darrell Wright
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

#include <iostream>
#include <string>
#include <utility>

#define BOOST_TEST_MODULE function_composition
#include <daw/boost_test.h>
#include <daw/daw_benchmark.h>
#include <daw/daw_size_literals.h>

#include "function_stream.h"

using namespace daw::size_literals;

BOOST_AUTO_TEST_CASE( composable_function_stream_test_001 ) {
	daw::get_task_scheduler( );

	auto const make_values = []( size_t howmany ) {
		std::vector<int> vs( howmany );
		std::generate( vs.begin( ), vs.end( ), std::rand );
		return vs;
	};

	auto const sort_values = []( auto values ) {
		std::sort( values.begin( ), values.end( ) );
		return std::move( values );
	};

	auto const odd_values = []( auto values ) {
		values.erase( std::remove_if( values.begin( ), values.end( ),
		                              []( auto const &i ) { return i % 2 == 0; } ),
		              values.end( ) );
		return std::move( values );
	};

	auto const sum_values = []( auto values ) {
		intmax_t r = 0;
		for( auto const &v : values ) {
			r += v;
		}
		return r;
	};

	auto const show_value = []( auto value ) {
		std::cout << value << '\n';
		return std::move( value );
	};

	auto const values = make_values( 75_MB );

	auto t1 = daw::benchmark( [&]( ) {
		auto const par_comp = daw::compose_future( ) | sort_values | odd_values |
		                      sum_values | show_value;
		wait_for_function_streams( par_comp( values ), par_comp( values ),
		                           par_comp( values ), par_comp( values ) );
	} );

	std::cout << "Parallel stream time " << daw::utility::format_seconds( t1, 2 )
	          << '\n';

	auto t2 = daw::benchmark( [&]( ) {
		auto const seq_comp =
		  daw::compose( ) | sort_values | odd_values | sum_values | show_value;
		seq_comp( values );
		seq_comp( values );
		seq_comp( values );
		seq_comp( values );
	} );

	std::cout << "Sequential time " << daw::utility::format_seconds( t2, 2 )
	          << '\n';
	std::cout << "Diff " << ( t2 / t1 ) << '\n';

	constexpr auto const do_nothing = daw::compose( );
	do_nothing( );
	auto const func =
	  daw::compose_future( ) | sort_values | odd_values | sum_values | show_value;
	auto result = func( values );
	result.wait( );
}

struct A {
	constexpr int operator( )( int x ) const noexcept {
		return x * 2;
	}
};

struct B {
	constexpr int operator( )( int x ) const noexcept {
		return x * 3;
	}
};

struct C {
	constexpr int operator( )( int x ) const noexcept {
		return x * 4;
	}
};

BOOST_AUTO_TEST_CASE( composable_function_stream_test_002 ) {
	constexpr auto fs = daw::compose_future( ) | A{} | B{} | C{};
	auto fut = fs( 3 );
	auto result = fut.get( );
	std::cout << result << '\n';
}

BOOST_AUTO_TEST_CASE( composable_function_stream_test_003 ) {
	constexpr auto fs = daw::compose_future( ) | A{} | B{} | C{};
	constexpr auto fs2 = daw::compose_future( ) | A{} | B{} | C{};
	constexpr auto fs3 = fs.join( fs2 );
	auto fut1 = fs( 3 );
	auto fut2 = fs2( 3 );
	auto fut3 = fs3( 3 );

	std::cout << fut1.get( ) << ", " << fut2.get( ) << ", " << fut3.get( )
	          << '\n';
}

BOOST_AUTO_TEST_CASE( composable_function_stream_test_004 ) {
	constexpr auto fs = daw::compose_future( ) | A{} | B{} | C{};
	constexpr auto fs2 = daw::compose_future( ) | A{} | B{} | C{};
	constexpr auto fs3 = fs | fs2 | A{} | fs;
	auto fut1 = fs( 3 );
	auto fut2 = fs2( 3 );
	auto fut3 = fs3( 3 );

	std::cout << fut1.get( ) << ", " << fut2.get( ) << ", " << fut3.get( )
	          << '\n';
}
