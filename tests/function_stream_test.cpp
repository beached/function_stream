// The MIT License (MIT)
//
// Copyright (c) 2016-2019 Darrell Wright
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

	constexpr uintmax_t fib( double n ) noexcept {
		uintmax_t last = 0;
		uintmax_t result = 1;
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

	bool function_composer_test( ) {
		daw::impl::function_composer_t<A, B, D> fc{A{}, B{}, D{}};
		static_assert(
		  std::is_same<decltype( fc.apply( 3 ) ), decltype( D{}( 3 ) )>::value,
		  "function_composer_t is not returning the correct type" );
		auto const result = fc.apply( 4 );
		daw::expecting( result == "Hello" );
		return true;
	}

	bool function_stream_test_001( ) {
		constexpr auto fs = daw::make_function_stream( &a, &b, &c );
		auto result = fs( 1 ).get( );
		daw::expecting( result, 24 );
		return true;
	}

	template<typename T, typename... Ts>
	std::vector<T> create_vector( T &&value, Ts &&... values ) {
		return std::vector<T>{std::initializer_list<T>{
		  std::forward<T>( value ), std::forward<Ts>( values )...}};
	}

	bool function_stream_test_002( ) {
		constexpr auto fs2 = daw::make_function_stream( &fib, &fib );
		auto result = fs2( 3 ).get( );
		daw::expecting( result, 29 );

		auto const fib2 = []( ) { return fib( 10 ); };

		constexpr auto f_grp = daw::make_future_result_group( fib2, fib2 );
		auto v = f_grp.get( );
		static_assert( daw::tuple_size_v<decltype( v )> == 2 );
		std::cout << "Function Group\n";
		daw::expecting( *std::get<0>( v ) == *std::get<1>( v ) );
		std::cout << *std::get<0>( v ) << '\n';
		std::cout << *std::get<1>( v ) << '\n';
		return true;
	}
} // namespace part1

std::string blah( int i ) {
	return std::to_string( i );
}

void future_result_test_001( ) {
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

int main( ) {
	part1::function_composer_test( );
	part1::function_stream_test_001( );
	future_result_test_001( );
	part1::function_stream_test_002( );
}
