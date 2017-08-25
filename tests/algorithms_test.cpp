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
#include <numeric>
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
	auto result_1 = daw::benchmark( [&]( ) { daw::algorithm::parallel::for_each( a.begin( ), a.end( ), find_even ); } );
	auto result_2 = daw::benchmark( [&]( ) {
		for( auto const &item : a ) {
			find_even( item );
		}
	} );
	auto result_3 = daw::benchmark( [&]( ) { daw::algorithm::parallel::for_each( a.begin( ), a.end( ), find_even ); } );
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
	auto result_1 = daw::benchmark( [&]( ) { daw::algorithm::parallel::fill( a.begin( ), a.end( ), 1 ); } );
	auto result_2 = daw::benchmark( [&]( ) { std::fill( a.begin( ), a.end( ), 2 ); } );
	auto result_3 = daw::benchmark( [&]( ) { daw::algorithm::parallel::fill( a.begin( ), a.end( ), 3 ); } );
	auto result_4 = daw::benchmark( [&]( ) { std::fill( a.begin( ), a.end( ), 4 ); } );
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

template<typename Iterator, typename T>
void fill_random( Iterator first, Iterator last, T minimum, T maximum ) {
	std::random_device rnd_device;
	// Specify the engine and distribution.
	std::mt19937 mersenne_engine{rnd_device( )};
	std::uniform_int_distribution<int64_t> dist{minimum, maximum};

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
	fill_random( a.begin( ), a.end( ) );
	auto b = a;
	auto result_1 = daw::benchmark( [&a]( ) { daw::algorithm::parallel::sort( a.begin( ), a.end( ) ); } );
	test_sort( a.begin( ), a.end( ), "p_result_1" );
	a = b;
	auto result_2 = daw::benchmark( [&a]( ) { std::sort( a.begin( ), a.end( ) ); } );
	test_sort( a.begin( ), a.end( ), "s_result_1" );
	a = b;
	auto result_3 = daw::benchmark( [&a]( ) { daw::algorithm::parallel::sort( a.begin( ), a.end( ) ); } );
	test_sort( a.begin( ), a.end( ), "p_result2" );
	a = b;
	auto result_4 = daw::benchmark( [&a]( ) { std::sort( a.begin( ), a.end( ) ); } );
	test_sort( a.begin( ), a.end( ), "s_result2" );
	auto const par_min = std::min( result_1, result_3 );
	auto const seq_min = std::min( result_2, result_4 );
	display_info( seq_min, par_min, SZ, sizeof( int64_t ), "sort" );
}

void stable_sort_test( size_t SZ ) {
	std::vector<int64_t> a;
	a.resize( SZ );
	fill_random( a.begin( ), a.end( ) );
	auto b = a;
	auto result_1 = daw::benchmark( [&a]( ) { daw::algorithm::parallel::stable_sort( a.begin( ), a.end( ) ); } );
	test_sort( a.begin( ), a.end( ), "p_result_1" );
	a = b;
	auto result_2 = daw::benchmark( [&a]( ) { std::stable_sort( a.begin( ), a.end( ) ); } );
	test_sort( a.begin( ), a.end( ), "s_result_1" );
	a = b;
	auto result_3 = daw::benchmark( [&a]( ) { daw::algorithm::parallel::stable_sort( a.begin( ), a.end( ) ); } );
	test_sort( a.begin( ), a.end( ), "p_result2" );
	a = b;
	auto result_4 = daw::benchmark( [&a]( ) { std::stable_sort( a.begin( ), a.end( ) ); } );
	test_sort( a.begin( ), a.end( ), "s_result2" );
	auto const par_min = std::min( result_1, result_3 );
	auto const seq_min = std::min( result_2, result_4 );
	display_info( seq_min, par_min, SZ, sizeof( int64_t ), "stable_sort" );
}

template<typename T>
void reduce_test( size_t SZ ) {
	std::vector<T> a;
	a.resize( SZ );
	std::fill( a.begin( ), a.end( ), 1 );
	auto b = a;
	int64_t accum_result1 = 0;
	int64_t accum_result2 = 0;
	auto result_1 =
	    daw::benchmark( [&]( ) { accum_result1 = daw::algorithm::parallel::reduce( a.begin( ), a.end( ), 0 ); } );
	a = b;
	auto result_2 = daw::benchmark( [&]( ) { accum_result2 = std::accumulate( a.begin( ), a.end( ), 0 ); } );
	daw::exception::daw_throw_on_false( accum_result1 == accum_result2, "Wrong return value" );
	a = b;
	auto result_3 =
	    daw::benchmark( [&]( ) { accum_result1 = daw::algorithm::parallel::reduce( a.begin( ), a.end( ), 0 ); } );
	a = b;
	auto result_4 = daw::benchmark( [&]( ) { accum_result2 = std::accumulate( a.begin( ), a.end( ), 0 ); } );
	daw::exception::daw_throw_on_false( accum_result1 == accum_result2, "Wrong return value" );
	auto const par_min = std::min( result_1, result_3 );
	auto const seq_min = std::min( result_2, result_4 );
	display_info( seq_min, par_min, SZ, sizeof( T ), "reduce" );
}

template<typename value_t, typename BinaryOp>
void reduce_test2( size_t SZ, value_t init, BinaryOp bin_op ) {
	std::vector<value_t> a;
	a.resize( SZ );
	fill_random( a.begin( ), a.end( ), 1, 4 );
	auto b = a;
	value_t accum_result1 = 0;
	value_t accum_result2 = 0;

	auto result_1 = daw::benchmark(
	    [&]( ) { accum_result1 = daw::algorithm::parallel::reduce<value_t>( a.begin( ), a.end( ), init, bin_op ); } );
	a = b;
	auto result_2 = daw::benchmark( [&]( ) { accum_result2 = std::accumulate( a.begin( ), a.end( ), init, bin_op ); } );
	daw::exception::daw_throw_on_false( accum_result1 == accum_result2, "Wrong return value" );
	a = b;
	auto result_3 = daw::benchmark(
	    [&]( ) { accum_result1 = daw::algorithm::parallel::reduce<value_t>( a.begin( ), a.end( ), init, bin_op ); } );
	a = b;
	auto result_4 = daw::benchmark( [&]( ) { accum_result2 = std::accumulate( a.begin( ), a.end( ), init, bin_op ); } );
	daw::exception::daw_throw_on_false( accum_result1 == accum_result2, "Wrong return value" );
	auto const par_min = std::min( result_1, result_3 );
	auto const seq_min = std::min( result_2, result_4 );
	display_info( seq_min, par_min, SZ, sizeof( value_t ), "reduce2" );
}

template<typename value_t>
void min_element_test( size_t SZ ) {
	std::vector<value_t> a;
	a.resize( SZ );
	fill_random( a.begin( ), a.end( ), std::numeric_limits<value_t>::min( ), std::numeric_limits<value_t>::max( ) );
	auto b = a;
	value_t min_result1 = 0;
	value_t min_result2 = 0;

	auto result_1 =
	    daw::benchmark( [&]( ) { min_result1 = *daw::algorithm::parallel::min_element( a.begin( ), a.end( ) ); } );
	a = b;
	auto result_2 = daw::benchmark( [&]( ) { min_result2 = *std::min_element( a.begin( ), a.end( ) ); } );
	daw::exception::daw_throw_on_false( min_result1 == min_result2, "Wrong return value" );
	a = b;
	auto result_3 =
	    daw::benchmark( [&]( ) { min_result1 = *daw::algorithm::parallel::min_element( a.begin( ), a.end( ) ); } );
	a = b;
	auto result_4 = daw::benchmark( [&]( ) { min_result2 = *std::min_element( a.begin( ), a.end( ) ); } );
	daw::exception::daw_throw_on_false( min_result1 == min_result2, "Wrong return value" );
	auto const par_min = std::min( result_1, result_3 );
	auto const seq_min = std::min( result_2, result_4 );
	display_info( seq_min, par_min, SZ, sizeof( value_t ), "min_element" );
}

template<typename value_t>
void max_element_test( size_t SZ ) {
	std::vector<value_t> a;
	a.resize( SZ );
	fill_random( a.begin( ), a.end( ), std::numeric_limits<value_t>::max( ), std::numeric_limits<value_t>::max( ) );
	auto b = a;
	value_t max_result1 = 0;
	value_t max_result2 = 0;

	auto result_1 =
	    daw::benchmark( [&]( ) { max_result1 = *daw::algorithm::parallel::max_element( a.begin( ), a.end( ) ); } );
	a = b;
	auto result_2 = daw::benchmark( [&]( ) { max_result2 = *std::max_element( a.begin( ), a.end( ) ); } );
	daw::exception::daw_throw_on_false( max_result1 == max_result2, "Wrong return value" );
	a = b;
	auto result_3 =
	    daw::benchmark( [&]( ) { max_result1 = *daw::algorithm::parallel::max_element( a.begin( ), a.end( ) ); } );
	a = b;
	auto result_4 = daw::benchmark( [&]( ) { max_result2 = *std::max_element( a.begin( ), a.end( ) ); } );
	daw::exception::daw_throw_on_false( max_result1 == max_result2, "Wrong return value" );
	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "max_element" );
}

int main( int, char ** ) {
	size_t const MAX_ITEMS = 100'000'000;
	auto ts = daw::get_task_scheduler( );

	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    for_each_test<double>( n );
	}
	std::cout << "int64_t\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    for_each_test<int64_t>( n );
	}
	std::cout << "int32_t\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    for_each_test<int32_t>( n );
	}
	std::cout << "double\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    fill_test<double>( n );
	}
	std::cout << "int64_t\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    fill_test<int64_t>( n );
	}
	std::cout << "int32_t\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    fill_test<int32_t>( n );
	}
	std::cout << "sort tests\n";
	std::cout << "int64_t\n";
	sort_test( 500'000'000 );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    sort_test( n );
	}

	std::cout << "stable_sort tests\n";
	std::cout << "int64_t\n";
	stable_sort_test( 500'000'000 );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    stable_sort_test( n );
	}

	std::cout << "reduce tests\n";
	std::cout << "int64_t\n";
	reduce_test<int64_t>( 500'000'000 );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    reduce_test<int64_t>( n );
	}

	std::cout << "double\n";
	reduce_test<double>( 500'000'000 );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    reduce_test<double>( n );
	}

	std::cout << "reduce2 tests\n";
	std::cout << "uint64_t\n";
	auto const bin_op = []( auto const &lhs, auto const &rhs ) noexcept {
	    return lhs*rhs;
	};
	reduce_test2<uint64_t>( 500'000'000, 1, bin_op );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
	    reduce_test2<uint64_t>( n, 1, bin_op );
	}

	std::cout << "min_element tests\n";
	std::cout << "int64_t\n";
	min_element_test<int64_t>( 500'000'000 );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		min_element_test<int64_t>( n );
	}

	std::cout << "max_element tests\n";
	std::cout << "int64_t\n";
	max_element_test<int64_t>( 500'000'000 );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		max_element_test<int64_t>( n );
	}

	return EXIT_SUCCESS;
}