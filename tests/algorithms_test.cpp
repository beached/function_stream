// The MIT License (MIT)
//
// Copyright (c) 2017 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>

#include <daw/daw_benchmark.h>

#include "algorithms.h"

template<typename T>
void for_each_test( size_t SZ ) {
	bool found = false;
	std::vector<T> a;
	a.reserve( SZ );
	for( size_t n = 0; n < SZ; ++n ) {
		a.emplace_back( 1 );
	}
	a[SZ / 2] = 4;
	auto find_even = [&]( int x ) {
		if( x % 2 == 0 ) {
			found = true;
		}
	};
	auto result_1 =
	    daw::benchmark( [&]( ) { daw::algorithm::parallel::for_each( a.data( ), a.data( ) + a.size( ), find_even ); } );
	auto result_2 = daw::benchmark( [&]( ) {
		for( auto const &item : a ) {
			find_even( item );
		}
	} );
	auto result_3 =
	    daw::benchmark( [&]( ) { daw::algorithm::parallel::for_each( a.data( ), a.data( ) + a.size( ), find_even ); } );
	auto result_4 = daw::benchmark( [&]( ) {
		for( auto const &item : a ) {
			find_even( item );
		}
	} );
	auto par_avg = ( result_1 + result_3 ) / 2;
	auto seq_avg = ( result_2 + result_4 ) / 2;
	std::cout << "for_each -> size: " << SZ << " found: " << found << " par test: " << par_avg
	          << " per item: " << ( par_avg / SZ ) << " seq_test: " << seq_avg << " per item: " << ( seq_avg / SZ )
	          << '\n';
}

template<typename T>
void fill_test( size_t SZ ) {
	std::vector<T> a;
	a.resize( SZ );
	auto result_1 = daw::benchmark( [&]( ) { daw::algorithm::parallel::fill( a.data( ), a.data( ) + a.size( ), 1 ); } );
	auto result_2 = daw::benchmark( [&]( ) { std::fill( a.data( ), a.data( ) + a.size( ), 2 ); } );
	auto result_3 = daw::benchmark( [&]( ) { daw::algorithm::parallel::fill( a.data( ), a.data( ) + a.size( ), 3 ); } );
	auto result_4 = daw::benchmark( [&]( ) { std::fill( a.data( ), a.data( ) + a.size( ), 4 ); } );
	auto par_avg = ( result_1 + result_3 ) / 2;
	auto seq_avg = ( result_2 + result_4 ) / 2;
	std::cout << "fill -> size: " << SZ << " par test: " << par_avg << " per item: " << ( par_avg / SZ )
	          << " seq_test: " << seq_avg << " per item: " << ( seq_avg / SZ ) << '\n';
}

template<typename Iterator>
void fill_random( Iterator first, Iterator last ) {
	std::random_device rnd_device;
	// Specify the engine and distribution.
	std::mt19937 mersenne_engine{rnd_device( )};
	std::uniform_int_distribution<int64_t> dist{0, std::distance( first, last )*2 };

	std::generate( first, last, [&]( ) { return dist( mersenne_engine ); } );
}

void sort_test( size_t SZ ) {
	std::vector<int64_t> a;
	a.resize( SZ );
	fill_random( a.data( ), a.data( ) + a.size( ) );
	auto b = a;
	auto result_1 = daw::benchmark( [&a]( ) { daw::algorithm::parallel::sort( a.data( ), a.data( ) + a.size( ) ); } );
	if( !std::is_sorted( a.cbegin( ), a.cend( ) ) ) {
		for( size_t n=0; n<a.size( ); ++n ) {
			std::cout << n << ' ' << a[n] << '\n';
		}
		std::cout << '\n';
	}
	daw::exception::daw_throw_on_false( std::is_sorted( a.data( ), a.data( ) + a.size( ) ), "Sequence not sorted" );
	a = b;
	auto result_2 = daw::benchmark( [&a]( ) { std::sort( a.data( ), a.data( ) + a.size( ) ); } );
	daw::exception::daw_throw_on_false( std::is_sorted( a.data( ), a.data( ) + a.size( ) ), "Sequence not sorted" );
	a = b;
	auto result_3 = daw::benchmark( [&a]( ) { daw::algorithm::parallel::sort( a.data( ), a.data( ) + a.size( ) ); } );
	daw::exception::daw_throw_on_false( std::is_sorted( a.data( ), a.data( ) + a.size( ) ), "Sequence not sorted" );
	a = b;
	auto result_4 = daw::benchmark( [&a]( ) { std::sort( a.data( ), a.data( ) + a.size( ) ); } );
	daw::exception::daw_throw_on_false( std::is_sorted( a.data( ), a.data( ) + a.size( ) ), "Sequence not sorted" );
	auto par_avg = ( result_1 + result_3 ) / 2;
	auto seq_avg = ( result_2 + result_4 ) / 2;
	std::cout << "sort -> size: " << SZ << " par test: " << par_avg << " per item: " << ( par_avg / SZ )
	          << " seq_test: " << seq_avg << " per item: " << ( seq_avg / SZ ) << '\n';
}

int main( int, char ** ) {
	auto ts = daw::get_task_scheduler( );
	for_each_test<double>( 100000000 );
	for_each_test<double>( 10000000 );
	for_each_test<double>( 1000000 );
	for_each_test<double>( 100000 );
	for_each_test<double>( 10000 );
	for_each_test<double>( 100 );
	std::cout << "int32_t\n";
	for_each_test<int32_t>( 100000000 );
	for_each_test<int32_t>( 10000000 );
	for_each_test<int32_t>( 1000000 );
	for_each_test<int32_t>( 100000 );
	for_each_test<int32_t>( 10000 );
	for_each_test<int32_t>( 100 );

	std::cout << "double\n";
	fill_test<double>( 100000000 );
	fill_test<double>( 10000000 );
	fill_test<double>( 1000000 );
	fill_test<double>( 100000 );
	fill_test<double>( 10000 );
	fill_test<double>( 100 );
	std::cout << "int32_t\n";
	fill_test<int32_t>( 100000000 );
	fill_test<int32_t>( 10000000 );
	fill_test<int32_t>( 1000000 );
	fill_test<int32_t>( 100000 );
	fill_test<int32_t>( 10000 );
	fill_test<int32_t>( 100 );

	sort_test( 32 );
	sort_test( 1024 );
	sort_test( 16384 );
	sort_test( 131072 );
	sort_test( 1048576 );
	sort_test( 16777216 );
	sort_test( 134217728 );

	return EXIT_SUCCESS;
}
