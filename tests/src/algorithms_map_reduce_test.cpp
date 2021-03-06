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
#include <numeric>
#include <vector>

#include <daw/daw_benchmark.h>
#include <daw/daw_random.h>

#include "daw/fs/algorithms.h"

#include "common.h"

template<typename value_t>
void map_reduce_test( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	// auto a = daw::make_random_data<value_t>( SZ );
	std::vector<value_t> a;
	a.resize( SZ );
	// fill_random( a.begin( ), a.end( ), -1, 1 );
	std::fill( a.begin( ), a.end( ), 1 );
	auto b = a;

	auto const map_function = []( value_t const &value ) {
		return value * value;
	};
	auto const reduce_function = []( value_t const &lhs, value_t const &rhs ) {
		return lhs + rhs;
	};

	value_t mr_value1 = 0;
	value_t mr_value2 = 0;

	auto const result_1 = daw::benchmark( [&]( ) {
		mr_value1 = daw::algorithm::parallel::map_reduce(
		  a.cbegin( ), a.cend( ), map_function, reduce_function, ts );
		daw::do_not_optimize( mr_value1 );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		std::transform( b.cbegin( ), b.cend( ), b.begin( ), map_function );
		auto start_it = std::next( b.cbegin( ) );
		mr_value2 =
		  std::accumulate( start_it, b.cend( ), *b.cbegin( ), reduce_function );
		daw::do_not_optimize( mr_value2 );
	} );
	daw::expecting( mr_value1, mr_value2 );

	b = a;
	mr_value1 = 0;
	mr_value2 = 0;

	auto const result_3 = daw::benchmark( [&]( ) {
		mr_value1 = daw::algorithm::parallel::map_reduce(
		  a.cbegin( ), a.cend( ), map_function, reduce_function, ts );
		daw::do_not_optimize( mr_value1 );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		std::transform( b.cbegin( ), b.cend( ), b.begin( ), map_function );
		auto start_it = std::next( b.cbegin( ) );
		mr_value2 =
		  std::accumulate( start_it, b.cend( ), *b.cbegin( ), reduce_function );
		daw::do_not_optimize( mr_value2 );
	} );
	daw::expecting( mr_value1, mr_value2 );

	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "map_reduce" );
}

template<typename value_t>
void map_reduce_test3( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	auto a = daw::make_random_data<value_t>( SZ, 1, 10'000 );

	auto const map_function = []( value_t value ) {
		for( uintmax_t n = 1; n <= 10000; ++n ) {
			value = static_cast<intmax_t>( static_cast<uintmax_t>( value ) ^ n ) %
			        static_cast<intmax_t>( n );
			if( value <= 0 ) {
				value = 10;
			}
		}
		return value;
	};
	auto const reduce_function = []( value_t const &lhs, value_t const &rhs ) {
		return lhs + rhs;
	};

	auto const map_reduce = []( auto first, auto const last, auto const m_func,
	                            auto const r_func ) {
		auto result = r_func( m_func( *first ), m_func( *std::next( first ) ) );
		std::advance( first, 2 );

		for( ; first != last; ++first ) {
			result = r_func( result, m_func( *first ) );
		}
		return result;
	};

	value_t mr_value1 = 0;
	value_t mr_value2 = 0;

	auto const result_1 = daw::benchmark( [&]( ) {
		mr_value1 = daw::algorithm::parallel::map_reduce(
		  a.cbegin( ), a.cend( ), map_function, reduce_function, ts );
		daw::do_not_optimize( mr_value1 );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		mr_value2 =
		  map_reduce( a.cbegin( ), a.cend( ), map_function, reduce_function );
		daw::do_not_optimize( mr_value2 );
	} );
	daw::expecting( mr_value1, mr_value2 );

	mr_value1 = 0;
	mr_value2 = 0;

	auto const result_3 = daw::benchmark( [&]( ) {
		mr_value1 = daw::algorithm::parallel::map_reduce(
		  a.cbegin( ), a.cend( ), map_function, reduce_function, ts );
		daw::do_not_optimize( mr_value1 );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		mr_value2 =
		  map_reduce( a.cbegin( ), a.cend( ), map_function, reduce_function );
		daw::do_not_optimize( mr_value1 );
	} );
	daw::expecting( mr_value1, mr_value2 );

	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "map_reduce3" );
}

template<typename value_t>
void map_reduce_test2( size_t SZ ) {
	auto ts = daw::get_task_scheduler( );
	auto a = daw::make_random_data<value_t>( SZ, 1, 10'000 );

	auto const map_function = []( value_t const &value ) {
		return value * value;
	};
	auto const reduce_function = []( value_t const &lhs, value_t const &rhs ) {
		return lhs + rhs;
	};

	auto const map_reduce = []( auto first, auto const last, auto const m_func,
	                            auto const r_func ) {
		auto result = r_func( m_func( *first ), m_func( *std::next( first ) ) );
		std::advance( first, 2 );

		for( ; first != last; ++first ) {
			result = r_func( result, m_func( *first ) );
		}
		return result;
	};

	value_t mr_value1 = 0;
	value_t mr_value2 = 0;

	auto const result_1 = daw::benchmark( [&]( ) {
		mr_value1 = daw::algorithm::parallel::map_reduce(
		  a.cbegin( ), a.cend( ), map_function, reduce_function, ts );
		daw::do_not_optimize( mr_value1 );
	} );
	auto const result_2 = daw::benchmark( [&]( ) {
		mr_value2 =
		  map_reduce( a.cbegin( ), a.cend( ), map_function, reduce_function );
		daw::do_not_optimize( mr_value2 );
	} );
	daw::expecting( mr_value1, mr_value2 );

	mr_value1 = 0;
	mr_value2 = 0;

	auto const result_3 = daw::benchmark( [&]( ) {
		mr_value1 = daw::algorithm::parallel::map_reduce(
		  a.cbegin( ), a.cend( ), map_function, reduce_function, ts );
		daw::do_not_optimize( mr_value1 );
	} );
	auto const result_4 = daw::benchmark( [&]( ) {
		mr_value2 =
		  map_reduce( a.cbegin( ), a.cend( ), map_function, reduce_function );
		daw::do_not_optimize( mr_value1 );
	} );
	daw::expecting( mr_value1, mr_value2 );

	auto const par_max = std::max( result_1, result_3 );
	auto const seq_max = std::max( result_2, result_4 );
	display_info( seq_max, par_max, SZ, sizeof( value_t ), "map_reduce2" );
}

void map_reduce_int64_t( ) {
	std::cout << "map_reduce tests - int64_t\n";
	for( size_t n = 128; n < MAX_ITEMS * 2; n *= 2 ) {
		map_reduce_test<int64_t>( n );
	}
	map_reduce_test<int64_t>( LARGE_TEST_SZ * 10 );
}

void map_reduce2_int64_t( ) {
	std::cout << "map_reduce3 tests - int64_t\n";
	for( size_t n = 128; n < MAX_ITEMS * 2; n *= 2 ) {
		map_reduce_test2<int64_t>( n );
	}
	map_reduce_test2<int64_t>( LARGE_TEST_SZ * 10 );
}

void map_reduce3_int64_t( ) {
	std::cout << "map_reduce3 tests - int64_t\n";
	for( size_t n = 128; n < MAX_ITEMS * 2; n *= 2 ) {
		map_reduce_test3<int64_t>( n );
	}
}

int main( ) {
	map_reduce_int64_t( );
	map_reduce2_int64_t( );
	map_reduce3_int64_t( );
}
