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

#define BOOST_TEST_MODULE function_stream
#include <daw/boost_test.h>
#include <daw/daw_benchmark.h>
#include <daw/daw_random.h>
#include <daw/daw_size_literals.h>

#include "daw/fs/function_stream.h"
#include "daw/fs/future_result.h"

using namespace daw::size_literals;

namespace part1 {
	constexpr double operator"" _R( long double d ) {
		return static_cast<double>( d );
	}

	constexpr double fib( double n ) noexcept {
		double last = 0;
		double result = 1;
		for( uintmax_t m = 1; m < n; ++m ) {
			auto new_last = result;
			result += result + last;
			last = new_last;
		}
		return result;
	}

	constexpr int a( int x ) noexcept {
		return x * 2;
	}

	constexpr int b( int x ) noexcept {
		return x * 3;
	}

	constexpr int c( int x ) noexcept {
		return x * 4;
	}

	struct A {
		constexpr int operator( )( int x ) const noexcept {
			return 1;
		}
	};

	struct B {
		constexpr int operator( )( int x ) const noexcept {
			return 2;
		}
	};

	struct C {
		void operator( )( std::string const & ) const noexcept {}
	};

	struct D {
		std::string operator( )( int x ) const {
			return std::string{"Hello"};
		}
	};

	BOOST_AUTO_TEST_CASE( function_composer_test ) {
		daw::impl::function_composer_t<A, B, D> fc{A{}, B{}, D{}};
		static_assert(
		  std::is_same<decltype( fc.apply( 3 ) ), decltype( D{}( 3 ) )>::value,
		  "function_composer_t is not returning the correct type" );
		std::cout << fc.apply( 4 ) << std::endl;
	}

	BOOST_AUTO_TEST_CASE( function_stream_test_001 ) {
		constexpr auto fs = daw::make_function_stream( &a, &b, &c );
		std::cout << fs( 1 ).get( ) << std::endl;
	}

	template<typename T, typename... Ts>
	std::vector<T> create_vector( T &&value, Ts &&... values ) {
		return std::vector<T>{std::initializer_list<T>{
		  std::forward<T>( value ), std::forward<Ts>( values )...}};
	}

	BOOST_AUTO_TEST_CASE( function_stream_test_002 ) {
		constexpr auto fs2 = daw::make_function_stream( &fib, &fib );
		auto results = create_vector( fs2( 3 ) );

		for( size_t n = 1; n < 40000; ++n ) {
			results.push_back( fs2( daw::randint( 5, 7 ) ) );
		};

		for( auto const &v : results ) {
			std::cout << "'" << v.get( ) << "'\n";
		}

		auto const fib2 = []( ) { return fib( 20 ); };

		auto f_grp = daw::make_future_result_group( fib2, fib2 ).get( );
		std::cout << "Function Group\n";
		std::cout << *std::get<0>( f_grp ) << '\n';
		std::cout << *std::get<1>( f_grp ) << '\n';
	}
} // namespace part1

std::string blah( int i ) {
	return std::to_string( i );
}

BOOST_AUTO_TEST_CASE( future_result_test_001 ) {
	auto ts = daw::get_task_scheduler( );
	auto const t = daw::make_future_result( []( ) {
		               std::cout << "part1\n";
		               std::cout << std::endl;
		               return 2;
	               } )
	                 .next( []( int i ) {
		                 std::cout << "part" << i << '\n';
		                 std::cout << "hahaha\n";
		                 std::cout << std::endl;
	                 } );

	t.wait( );

	std::cout << "operator|\n";

	auto const u = daw::make_future_result( []( ) -> int {
		std::cout << "part1\n";
		std::cout << std::endl;
		return 2;
	} );

	auto const result = u | []( int i ) {
		std::cout << "part" << i << '\n';
		std::cout << "hahaha\n";
		std::cout << std::endl;
		return i + 1;
	} | blah | []( std::string s ) { std::cout << s << "\nfin\n"; };

	auto const v = daw::make_future_generator( []( ) {
		std::cout << "part1\n";
		std::cout << std::endl;
		return 2;
	} );

	auto const result2 = v | []( int i ) {
		std::cout << "part" << i << '\n';
		std::cout << "hahaha\n";
		std::cout << std::endl;
		return i + 1;
	} | blah | []( std::string s ) { std::cout << s << "\nfin\n"; };

	result2( ).wait( );
}
