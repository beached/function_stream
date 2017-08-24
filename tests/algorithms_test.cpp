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

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <date/chrono_io.h>
#include <date/date.h>
#include <iostream>
#include <random>
#include <vector>

#include "task_scheduler.h"
#include <daw/daw_benchmark.h>
#include <daw/daw_string_view.h>

#include "algorithms.h"

template<typename T>
double calc_speedup( T seq_time, T par_time ) {
	static double const max_speedup = daw::get_task_scheduler( ).size( );
	auto result = seq_time / par_time;
	result = result / max_speedup;
	return result * 100.0;
}

void display_info( double seq_time, double par_time, double count, size_t bytes, daw::string_view label ) {
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
		auto val = ( count * static_cast<double>( bytes ) ) / t;
		if( val < 1024 ) {
			return std::to_string( static_cast<uint64_t>( val * 100.0 ) / 100 ) + "bytes/s";
		}
		val /= 1024.0;
		if( val < 1024 ) {
			return std::to_string( static_cast<uint64_t>( val * 100.0 ) / 100 ) + "KB/s";
		}
		val /= 1024.0;
		if( val < 1024 ) {
			return std::to_string( static_cast<uint64_t>( val * 100.0 ) / 100 ) + "MB/s";
		}
		val /= 1024.0;
		return std::to_string( static_cast<uint64_t>( val * 100.0 ) / 100 ) + "GB/s";
	};

	std::cout << label << ": size->" << static_cast<uint64_t>( count ) << " %cpus->"
	          << calc_speedup( seq_time, par_time ) << " par_total->" << make_seconds( par_time, 1 ) << " par_item->"
	          << make_seconds( par_time, count ) << " throughput->" << mbs( par_time ) << " seq_total->"
	          << make_seconds( seq_time, 1 ) << " seq_item->" << make_seconds( seq_time, count ) << " throughput->"
	          << mbs( seq_time ) << '\n';
}

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
	auto const par_min = ( result_1 + result_3 ) / 2;
	auto const seq_min = ( result_2 + result_4 ) / 2;
	display_info( seq_min, par_min, SZ, sizeof( T ), "for_each" );
}

template<typename T>
void fill_test( size_t SZ ) {
	std::vector<T> a;
	a.resize( SZ );
	auto result_1 = daw::benchmark( [&]( ) { daw::algorithm::parallel::fill( a.data( ), a.data( ) + a.size( ), 1 ); } );
	auto result_2 = daw::benchmark( [&]( ) { std::fill( a.data( ), a.data( ) + a.size( ), 2 ); } );
	auto result_3 = daw::benchmark( [&]( ) { daw::algorithm::parallel::fill( a.data( ), a.data( ) + a.size( ), 3 ); } );
	auto result_4 = daw::benchmark( [&]( ) { std::fill( a.data( ), a.data( ) + a.size( ), 4 ); } );
	auto const par_min = ( result_1 + result_3 ) / 2;
	auto const seq_min = ( result_2 + result_4 ) / 2;
	display_info( seq_min, par_min, SZ, sizeof( T ), "fill" );
}

template<typename Iterator>
void fill_random( Iterator first, Iterator last ) {
	std::random_device rnd_device;
	// Specify the engine and distribution.
	std::mt19937 mersenne_engine{rnd_device( )};
	std::uniform_int_distribution<int64_t> dist{0, std::distance( first, last ) * 2};

	std::generate( first, last, [&]( ) { return dist( mersenne_engine ); } );
}

template<typename Iterator>
void test_sort( Iterator const first, Iterator const last, daw::string_view label ) {
	if( first == last ) {
		return;
	}
	auto it = first;
	auto last_val = *it;
	++it;
	for( ; it != last; ++it ) {
		if( *it < last_val ) {
			auto const pos = std::distance( first, it );
			std::cerr << "Sequence '" << label << "' not sorted at position (" << std::distance( first, it ) << '/'
			          << std::distance( first, last ) << ")\n";

			auto start = pos > 10 ? std::next( first, pos - 10 ) : first;
			auto const end = std::distance( it, last ) > 10 ? std::next( it, 10 ) : last;
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
	std::vector<int64_t> a;
	a.resize( SZ );
	fill_random( a.data( ), a.data( ) + a.size( ) );
	auto b = a;
	auto result_1 = daw::benchmark( [&a]( ) { daw::algorithm::parallel::sort( a.data( ), a.data( ) + a.size( ) ); } );
	test_sort( a.begin( ), a.end( ), "p_result_1" );
	a = b;
	auto result_2 = daw::benchmark( [&a]( ) { std::sort( a.data( ), a.data( ) + a.size( ) ); } );
	test_sort( a.begin( ), a.end( ), "s_result_1" );
	a = b;
	auto result_3 = daw::benchmark( [&a]( ) { daw::algorithm::parallel::sort( a.data( ), a.data( ) + a.size( ) ); } );
	test_sort( a.begin( ), a.end( ), "p_result2" );
	a = b;
	auto result_4 = daw::benchmark( [&a]( ) { std::sort( a.data( ), a.data( ) + a.size( ) ); } );
	test_sort( a.begin( ), a.end( ), "s_result2" );
	auto const par_min = std::min( result_1, result_3 );
	auto const seq_min = std::min( result_2, result_4 );
	display_info( seq_min, par_min, SZ, sizeof( int64_t ), "sort" );
}

void stable_sort_test( size_t SZ ) {
	std::vector<int64_t> a;
	a.resize( SZ );
	fill_random( a.data( ), a.data( ) + a.size( ) );
	auto b = a;
	auto result_1 = daw::benchmark( [&a]( ) { daw::algorithm::parallel::stable_sort( a.data( ), a.data( ) + a.size( ) ); } );
	test_sort( a.begin( ), a.end( ), "p_result_1" );
	a = b;
	auto result_2 = daw::benchmark( [&a]( ) { std::stable_sort( a.data( ), a.data( ) + a.size( ) ); } );
	test_sort( a.begin( ), a.end( ), "s_result_1" );
	a = b;
	auto result_3 = daw::benchmark( [&a]( ) { daw::algorithm::parallel::stable_sort( a.data( ), a.data( ) + a.size( ) ); } );
	test_sort( a.begin( ), a.end( ), "p_result2" );
	a = b;
	auto result_4 = daw::benchmark( [&a]( ) { std::stable_sort( a.data( ), a.data( ) + a.size( ) ); } );
	test_sort( a.begin( ), a.end( ), "s_result2" );
	auto const par_min = std::min( result_1, result_3 );
	auto const seq_min = std::min( result_2, result_4 );
	display_info( seq_min, par_min, SZ, sizeof( int64_t ), "sort" );
}


int main( int, char ** ) {
	auto ts = daw::get_task_scheduler( );
	/*
	for( size_t n = 100000000; n >= 100; n /= 10 ) {
		for_each_test<double>( n );
	}
	std::cout << "int64_t\n";
	for( size_t n = 100000000; n >= 100; n /= 10 ) {
		for_each_test<int64_t>( n );
	}
	std::cout << "int32_t\n";
	for( size_t n = 100000000; n >= 100; n /= 10 ) {
		for_each_test<int32_t>( n );
	}
	std::cout << "double\n";
	for( size_t n = 100000000; n >= 100; n /= 10 ) {
		fill_test<double>( n );
	}
	std::cout << "int64_t\n";
	for( size_t n = 100000000; n >= 100; n /= 10 ) {
		fill_test<int64_t>( n );
	}
	std::cout << "int32_t\n";
	for( size_t n = 100000000; n >= 100; n /= 10 ) {
		fill_test<int32_t>( n );
	}
	 */
	std::cout << "sort tests\n";
	std::cout << "int64_t\n";
	sort_test( 500000000 );
	for( size_t n = 100000000; n >= 100; n /= 10 ) {
		sort_test( n );
	}

	std::cout << "stable_sort tests\n";
	std::cout << "int64_t\n";
	sort_test( 500000000 );
	for( size_t n = 100000000; n >= 100; n /= 10 ) {
		stable_sort_test( n );
	}

	return EXIT_SUCCESS;
}
