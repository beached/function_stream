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
#include <date/chrono_io.h>
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

#define BOOST_TEST_MODULE parallel_algorithms
#include <daw/boost_test.h>

#include "algorithms.h"

#define USE_SORT_FJ

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

	template<typename T>
	void fill_test( size_t SZ ) {
		auto ts = daw::get_task_scheduler( );
		std::vector<T> a;
		a.resize( SZ );
		auto const result_1 = daw::benchmark( [&]( ) {
			daw::algorithm::parallel::fill( a.begin( ), a.end( ), 1, ts );
			daw::do_not_optimize( a );
		} );
		auto const result_2 = daw::benchmark( [&]( ) {
			std::fill( a.begin( ), a.end( ), 2 );
			daw::do_not_optimize( a );
		} );
		auto const result_3 = daw::benchmark( [&]( ) {
			daw::algorithm::parallel::fill( a.begin( ), a.end( ), 3, ts );
			daw::do_not_optimize( a );
		} );
		auto const result_4 = daw::benchmark( [&]( ) {
			std::fill( a.begin( ), a.end( ), 4 );
			daw::do_not_optimize( a );
		} );
		auto const par_min = ( result_1 + result_3 ) / 2;
		auto const seq_min = ( result_2 + result_4 ) / 2;
		display_info( seq_min, par_min, SZ, sizeof( T ), "fill" );
	}

	template<typename Iterator>
	void test_sort( Iterator const first, Iterator const last,
	                daw::string_view label ) {
		if( first == last ) {
			return;
		}
		auto it = first;
		auto last_val = *it;
		++it;
		for( ; it != last; ++it ) {
			if( *it < last_val ) {
				auto const pos = std::distance( first, it );
				std::cerr << "Sequence '" << label << "' not sorted at position ("
				          << std::distance( first, it ) << '/'
				          << std::distance( first, last ) << ")\n";

				auto start = pos > 10 ? std::next( first, pos - 10 ) : first;
				auto const end =
				  std::distance( it, last ) > 10 ? std::next( it, 10 ) : last;
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
		auto ts = daw::get_task_scheduler( );
		auto a = daw::make_random_data<int64_t>( SZ );

		/*
		    std::vector<int> a;
		    a.resize( SZ );
		    std::iota( a.begin( ), a.end( ), 0 );
		*/

		auto b = a;
		auto const par_test = [&]( ) {
			daw::algorithm::parallel::sort( a.begin( ), a.end( ), ts );
			daw::do_not_optimize( a );
		};

#ifdef USE_SORT_FJ
		auto const fj_test = [&]( ) {
			daw::algorithm::parallel::fork_join_sort(
			  a.data( ), a.data( ) + static_cast<ptrdiff_t>( a.size( ) ), ts );
			daw::do_not_optimize( a );
		};
#endif

		auto const ser_test = [&]( ) {
			std::sort( a.begin( ), a.end( ) );
			daw::do_not_optimize( a );
		};

		auto const par_result_1 = daw::benchmark( par_test );
		test_sort( a.begin( ), a.end( ), "p_result_1" );
		a = b;

#ifdef USE_SORT_FJ
		auto const fj_result_1 = daw::benchmark( fj_test );
		test_sort( a.begin( ), a.end( ), "p_result_1" );
		a = b;
#endif

		auto const ser_result_1 = daw::benchmark( ser_test );
		test_sort( a.begin( ), a.end( ), "s_result_1" );
		a = b;
		auto const par_result_2 = daw::benchmark( par_test );
		test_sort( a.begin( ), a.end( ), "p_result2" );
		a = b;

#ifdef USE_SORT_FJ
		auto const fj_result_2 = daw::benchmark( fj_test );
		test_sort( a.begin( ), a.end( ), "p_result_1" );
		a = b;
#endif

		auto const ser_result_2 = daw::benchmark( ser_test );
		test_sort( a.begin( ), a.end( ), "s_result2" );

		auto const par_min = std::min( par_result_1, par_result_2 );
		auto const seq_min = std::min( ser_result_1, ser_result_2 );
#ifdef USE_SORT_FJ
		auto const fj_min = std::min( fj_result_1, fj_result_2 );
#endif

		display_info( seq_min, par_min, SZ, sizeof( int64_t ), "sort" );
#ifdef USE_SORT_FJ
		display_info( seq_min, fj_min, SZ, sizeof( int64_t ), "sort_fj" );
#endif
	}

	void stable_sort_test( size_t SZ ) {
		auto ts = daw::get_task_scheduler( );
		auto a = daw::make_random_data<int64_t>( SZ );

		/*
		    std::vector<int> a;
		    a.resize( SZ );
		    std::iota( a.begin( ), a.end( ), 0 );
		*/

		auto b = a;
		auto const par_test = [&]( ) {
			daw::algorithm::parallel::stable_sort( a.begin( ), a.end( ), ts );
			daw::do_not_optimize( a );
		};

#ifdef USE_SORT_FJ
		auto const fj_test = [&]( ) {
			daw::algorithm::parallel::stable_fork_join_sort(
			  a.data( ), a.data( ) + static_cast<ptrdiff_t>( a.size( ) ), ts );
			daw::do_not_optimize( a );
		};
#endif

		auto const ser_test = [&]( ) {
			std::stable_sort( a.begin( ), a.end( ) );
			daw::do_not_optimize( a );
		};

		auto const par_result_1 = daw::benchmark( par_test );
		test_sort( a.begin( ), a.end( ), "p_result_1" );
		a = b;

#ifdef USE_SORT_FJ
		auto const fj_result_1 = daw::benchmark( fj_test );
		test_sort( a.begin( ), a.end( ), "p_result_1" );
		a = b;
#endif

		auto const ser_result_1 = daw::benchmark( ser_test );
		test_sort( a.begin( ), a.end( ), "s_result_1" );
		a = b;
		auto const par_result_2 = daw::benchmark( par_test );
		test_sort( a.begin( ), a.end( ), "p_result2" );
		a = b;

#ifdef USE_SORT_FJ
		auto const fj_result_2 = daw::benchmark( fj_test );
		test_sort( a.begin( ), a.end( ), "p_result_1" );
		a = b;
#endif

		auto const ser_result_2 = daw::benchmark( ser_test );
		test_sort( a.begin( ), a.end( ), "s_result2" );

		auto const par_min = std::min( par_result_1, par_result_2 );
		auto const seq_min = std::min( ser_result_1, ser_result_2 );
#ifdef USE_SORT_FJ
		auto const fj_min = std::min( fj_result_1, fj_result_2 );
#endif

		display_info( seq_min, par_min, SZ, sizeof( int64_t ), "stable_sort" );
#ifdef USE_SORT_FJ
		display_info( seq_min, fj_min, SZ, sizeof( int64_t ), "stable_sort_fj" );
#endif
	}

	template<typename T>
	void reduce_test( size_t SZ ) {
		auto ts = daw::get_task_scheduler( );
		std::vector<T> a;
		a.resize( SZ );
		std::fill( a.begin( ), a.end( ), 1 );
		auto b = a;
		T accum_result1 = 0;
		T accum_result2 = 0;
		auto const result_1 = daw::benchmark( [&]( ) {
			accum_result1 = daw::algorithm::parallel::reduce(
			  a.begin( ), a.end( ), static_cast<T>( 0 ), ts );
			daw::do_not_optimize( accum_result1 );
		} );
		a = b;
		auto const result_2 = daw::benchmark( [&]( ) {
			accum_result2 =
			  std::accumulate( a.begin( ), a.end( ), static_cast<T>( 0 ) );
			daw::do_not_optimize( accum_result2 );
		} );

		BOOST_REQUIRE_MESSAGE(
		  daw::math::nearly_equal( accum_result1, accum_result2 ),
		  "Wrong return value" );

		a = b;
		auto const result_3 = daw::benchmark( [&]( ) {
			accum_result1 = daw::algorithm::parallel::reduce(
			  a.begin( ), a.end( ), static_cast<T>( 0 ), ts );
			daw::do_not_optimize( accum_result1 );
		} );
		a = b;
		auto const result_4 = daw::benchmark( [&]( ) {
			accum_result2 =
			  std::accumulate( a.begin( ), a.end( ), static_cast<T>( 0 ) );
			daw::do_not_optimize( accum_result2 );
		} );

		BOOST_REQUIRE_MESSAGE(
		  daw::math::nearly_equal( accum_result1, accum_result2 ),
		  "Wrong return value" );

		auto const par_min = std::min( result_1, result_3 );
		auto const seq_min = std::min( result_2, result_4 );
		display_info( seq_min, par_min, SZ, sizeof( T ), "reduce" );
	}

	template<typename value_t, typename BinaryOp>
	void reduce_test2( size_t SZ, value_t init, BinaryOp bin_op ) {
		auto ts = daw::get_task_scheduler( );
		auto a = daw::make_random_data<value_t>( SZ );
		auto b = a;
		value_t accum_result1 = 0;
		value_t accum_result2 = 0;

		auto const result_1 = daw::benchmark( [&]( ) {
			accum_result1 = daw::algorithm::parallel::reduce<value_t>(
			  a.begin( ), a.end( ), init, bin_op, ts );
			daw::do_not_optimize( accum_result1 );
		} );
		a = b;
		auto const result_2 = daw::benchmark( [&]( ) {
			accum_result2 = std::accumulate( a.begin( ), a.end( ), init, bin_op );
			daw::do_not_optimize( accum_result2 );
		} );
		BOOST_REQUIRE_MESSAGE( accum_result1 == accum_result2,
		                       "Wrong return value" );
		a = b;
		auto const result_3 = daw::benchmark( [&]( ) {
			accum_result1 = daw::algorithm::parallel::reduce<value_t>(
			  a.begin( ), a.end( ), init, bin_op, ts );
			daw::do_not_optimize( accum_result1 );
		} );
		a = b;
		auto const result_4 = daw::benchmark( [&]( ) {
			accum_result2 = std::accumulate( a.begin( ), a.end( ), init, bin_op );
			daw::do_not_optimize( accum_result2 );
		} );
		BOOST_REQUIRE_MESSAGE( accum_result1 == accum_result2,
		                       "Wrong return value" );
		auto const par_min = std::min( result_1, result_3 );
		auto const seq_min = std::min( result_2, result_4 );
		display_info( seq_min, par_min, SZ, sizeof( value_t ), "reduce2" );
	}

	template<typename value_t>
	void min_element_test( size_t SZ ) {
		auto ts = daw::get_task_scheduler( );
		auto a = daw::make_random_data<value_t>( SZ );
		value_t min_result1 = 0;
		value_t min_result2 = 0;

		auto const result_1 = daw::benchmark( [&]( ) {
			min_result1 =
			  *daw::algorithm::parallel::min_element( a.begin( ), a.end( ), ts );
			daw::do_not_optimize( min_result1 );
		} );
		auto const result_2 = daw::benchmark( [&]( ) {
			min_result2 = *std::min_element( a.begin( ), a.end( ) );
			daw::do_not_optimize( min_result2 );
		} );
		BOOST_REQUIRE_MESSAGE( min_result1 == min_result2, "Wrong return value" );
		auto const result_3 = daw::benchmark( [&]( ) {
			min_result1 =
			  *daw::algorithm::parallel::min_element( a.begin( ), a.end( ), ts );
			daw::do_not_optimize( min_result1 );
		} );
		auto const result_4 = daw::benchmark( [&]( ) {
			min_result2 = *std::min_element( a.begin( ), a.end( ) );
			daw::do_not_optimize( min_result2 );
		} );
		BOOST_REQUIRE_MESSAGE( min_result1 == min_result2, "Wrong return value" );
		auto const par_min = std::min( result_1, result_3 );
		auto const seq_min = std::min( result_2, result_4 );
		display_info( seq_min, par_min, SZ, sizeof( value_t ), "min_element" );
	}

	template<typename value_t>
	void max_element_test( size_t SZ ) {
		auto ts = daw::get_task_scheduler( );
		auto a = daw::make_random_data<value_t>( SZ );
		value_t max_result1 = 0;
		value_t max_result2 = 0;

		auto const result_1 = daw::benchmark( [&]( ) {
			max_result1 =
			  *daw::algorithm::parallel::max_element( a.begin( ), a.end( ), ts );
			daw::do_not_optimize( max_result1 );
		} );
		auto const result_2 = daw::benchmark( [&]( ) {
			max_result2 = *std::max_element( a.begin( ), a.end( ) );
			daw::do_not_optimize( max_result2 );
		} );
		BOOST_REQUIRE_MESSAGE( max_result1 == max_result2, "Wrong return value" );
		auto const result_3 = daw::benchmark( [&]( ) {
			max_result1 =
			  *daw::algorithm::parallel::max_element( a.begin( ), a.end( ), ts );
			daw::do_not_optimize( max_result1 );
		} );
		auto const result_4 = daw::benchmark( [&]( ) {
			max_result2 = *std::max_element( a.begin( ), a.end( ) );
			daw::do_not_optimize( max_result2 );
		} );
		BOOST_REQUIRE_MESSAGE( max_result1 == max_result2, "Wrong return value" );
		auto const par_max = std::max( result_1, result_3 );
		auto const seq_max = std::max( result_2, result_4 );
		display_info( seq_max, par_max, SZ, sizeof( value_t ), "max_element" );
	}

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
		BOOST_REQUIRE_MESSAGE( mr_value1 == mr_value2, "Wrong return value" );

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
		BOOST_REQUIRE_MESSAGE( mr_value1 == mr_value2, "Wrong return value" );

		auto const par_max = std::max( result_1, result_3 );
		auto const seq_max = std::max( result_2, result_4 );
		display_info( seq_max, par_max, SZ, sizeof( value_t ), "map_reduce" );
	}

	template<typename value_t>
	void map_reduce_test3( size_t SZ ) {
		auto ts = daw::get_task_scheduler( );
		auto a = daw::make_random_data<value_t>( SZ, 1, 10'000 );

		auto const map_function = []( value_t value ) {
			for( intmax_t n = 1; n <= 10000; ++n ) {
				value = ( value ^ n ) % n;
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
		BOOST_REQUIRE_MESSAGE( mr_value1 == mr_value2, "Wrong return value" );

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
		BOOST_REQUIRE_MESSAGE( mr_value1 == mr_value2, "Wrong return value" );

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
		BOOST_REQUIRE_MESSAGE( mr_value1 == mr_value2, "Wrong return value" );

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
		BOOST_REQUIRE_MESSAGE( mr_value1 == mr_value2, "Wrong return value" );

		auto const par_max = std::max( result_1, result_3 );
		auto const seq_max = std::max( result_2, result_4 );
		display_info( seq_max, par_max, SZ, sizeof( value_t ), "map_reduce2" );
	}

	template<typename value_t>
	void scan_test( size_t SZ ) {
		auto ts = daw::get_task_scheduler( );
		auto a = daw::make_random_data<value_t>( SZ, -10, 10 );
		auto b = a;
		auto c = a;

		auto const reduce_function = []( auto lhs, auto rhs ) noexcept {
			volatile int x = 0;
			for( size_t n = 0; n < 50; ++n ) {
				x = x + 1;
			}
			return lhs + rhs;
		};

		auto const result_1 = daw::benchmark( [&]( ) {
			daw::algorithm::parallel::scan( a.data( ), a.data( ) + a.size( ),
			                                b.data( ), b.data( ) + b.size( ),
			                                reduce_function, ts );

			daw::do_not_optimize( b );
		} );
		auto const result_2 = daw::benchmark( [&]( ) {
			std::partial_sum( a.cbegin( ), a.cend( ), c.begin( ), reduce_function );
			daw::do_not_optimize( c );
		} );
		BOOST_REQUIRE_MESSAGE(
		  std::equal( b.cbegin( ), b.cend( ), c.cbegin( ), c.cend( ) ),
		  "Wrong return value" );
		b = a;
		c = a;
		auto const result_3 = daw::benchmark( [&]( ) {
			daw::algorithm::parallel::scan( a.cbegin( ), a.cend( ), b.begin( ),
			                                b.end( ), reduce_function, ts );
			daw::do_not_optimize( b );
		} );
		auto const result_4 = daw::benchmark( [&]( ) {
			std::partial_sum( a.cbegin( ), a.cend( ), c.begin( ), reduce_function );
			daw::do_not_optimize( c );
		} );
		BOOST_REQUIRE_MESSAGE(
		  std::equal( b.cbegin( ), b.cend( ), c.cbegin( ), c.cend( ) ),
		  "Wrong return value" );

		auto const par_max = std::max( result_1, result_3 );
		auto const seq_max = std::max( result_2, result_4 );
		display_info( seq_max, par_max, SZ, sizeof( value_t ), "scan" );
	}

	template<typename value_t>
	void find_if_test( size_t SZ ) {
		auto ts = daw::get_task_scheduler( );
		auto a = daw::make_random_data<value_t>( SZ, -50, 50 );

		auto const pos =
		  a.size( ) - 1; // daw::randint( static_cast<size_t>( 0 ), a.size( ) );
		a[pos] = 100;
		auto const pred = []( auto const &value ) noexcept {
			return value == 100;
		};

		auto it1 = a.cend( );
		auto it2 = a.cend( );
		auto const result_1 = daw::benchmark( [&]( ) {
			it1 =
			  daw::algorithm::parallel::find_if( a.cbegin( ), a.cend( ), pred, ts );
			daw::do_not_optimize( it1 );
		} );
		auto const result_2 = daw::benchmark( [&]( ) {
			it2 = std::find_if( a.cbegin( ), a.cend( ), pred );
			daw::do_not_optimize( it2 );
		} );
		BOOST_REQUIRE_MESSAGE( it1 == it2, "Wrong return value" );

		it1 = a.cend( );
		it2 = a.cend( );
		auto const result_3 = daw::benchmark( [&]( ) {
			it1 =
			  daw::algorithm::parallel::find_if( a.cbegin( ), a.cend( ), pred, ts );
			daw::do_not_optimize( it1 );
		} );
		auto const result_4 = daw::benchmark( [&]( ) {
			it2 = std::find_if( a.cbegin( ), a.cend( ), pred );
			daw::do_not_optimize( it2 );
		} );
		BOOST_REQUIRE_MESSAGE( it1 == it2, "Wrong return value" );

		auto const par_max = std::max( result_1, result_3 );
		auto const seq_max = std::max( result_2, result_4 );
		display_info( seq_max, par_max, SZ, sizeof( value_t ), "find_if" );
	}

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

	template<typename value_t>
	void count_test( size_t SZ ) {
		auto ts = daw::get_task_scheduler( );
		std::vector<value_t> a;
		a.resize( SZ );
		for( size_t n = 0; n < a.size( ); ++n ) {
			a[n] = static_cast<value_t>( n );
		}

		auto const pred = []( value_t val ) noexcept {
			return val % 2 == 0;
		};

		intmax_t x1 = 0;
		intmax_t x2 = 0;

		auto const result_1 = daw::benchmark( [&]( ) {
			x1 = daw::algorithm::parallel::count_if( a.cbegin( ), a.cend( ), pred );
			daw::do_not_optimize( x1 );
		} );

		auto const result_2 = daw::benchmark( [&]( ) {
			x2 = std::count_if( a.cbegin( ), a.cend( ), pred );
			daw::do_not_optimize( x2 );
		} );

		BOOST_REQUIRE_MESSAGE( x1 == x2, "Wrong return value" );

		x1 = 0;
		x2 = 0;

		auto const result_3 = daw::benchmark( [&]( ) {
			x1 = daw::algorithm::parallel::count_if( a.cbegin( ), a.cend( ), pred );
			daw::do_not_optimize( x1 );
		} );

		auto const result_4 = daw::benchmark( [&]( ) {
			x2 = std::count_if( a.cbegin( ), a.cend( ), pred );
			daw::do_not_optimize( x2 );
		} );

		BOOST_REQUIRE_MESSAGE( x1 == x2, "Wrong return value" );

		auto const par_max = std::max( result_1, result_3 );
		auto const seq_max = std::max( result_2, result_4 );
		display_info( seq_max, par_max, SZ, sizeof( value_t ), "count" );
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

BOOST_AUTO_TEST_CASE( fill_double ) {
	std::cout << "fill tests - double\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		fill_test<double>( n );
	}
}

BOOST_AUTO_TEST_CASE( fill_int64_t ) {
	std::cout << "fill tests - int64_t\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		fill_test<int64_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( fill_int32_t ) {
	std::cout << "fill tests - int32_t\n";
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		fill_test<int32_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( sort_int64_t ) {
	std::cout << "sort tests - int64_t\n";
	sort_test( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 10; n /= 10 ) {
		sort_test( n );
		std::cout << '\n';
	}
}

BOOST_AUTO_TEST_CASE( stable_sort_int64_t ) {
	std::cout << "stable_sort tests - int64_t\n";
	stable_sort_test( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 10; n /= 10 ) {
		stable_sort_test( n );
		std::cout << '\n';
	}
}

BOOST_AUTO_TEST_CASE( reduce_double ) {
	std::cout << "reduce tests - double\n";
	reduce_test<double>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		reduce_test<double>( n );
	}
}

BOOST_AUTO_TEST_CASE( reduce_int64_t ) {
	std::cout << "reduce tests - int64_t\n";
	reduce_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		reduce_test<int64_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( reduce2_int64_t ) {
	std::cout << "reduce 2 tests - uint64_t\n";
	auto const bin_op = []( auto const &lhs, auto const &rhs ) noexcept {
		return lhs * rhs;
	};
	reduce_test2<uint64_t>( LARGE_TEST_SZ, 1, bin_op );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		reduce_test2<uint64_t>( n, 1, bin_op );
	}
}

BOOST_AUTO_TEST_CASE( reduce3_double ) {
	std::cout << "reduce 3 tests - double\n";
	reduce_test<double>( LARGE_TEST_SZ*10 );
	reduce_test<double>( 6'000'000 );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		reduce_test<double>( n );
	}
}

BOOST_AUTO_TEST_CASE( min_element_int64_t ) {
	std::cout << "min_element tests - int64_t\n";
	min_element_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		min_element_test<int64_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( max_element_int64_t ) {
	std::cout << "max_element tests - int64_t\n";
	max_element_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		max_element_test<int64_t>( n );
	}
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

BOOST_AUTO_TEST_CASE( map_reduce_int64_t ) {
	std::cout << "map_reduce tests - int64_t\n";
	map_reduce_test<int64_t>( LARGE_TEST_SZ * 10 );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		map_reduce_test<int64_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( map_reduce2_int64_t ) {
	std::cout << "map_reduce3 tests - int64_t\n";
	map_reduce_test2<int64_t>( LARGE_TEST_SZ * 10 );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		map_reduce_test2<int64_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( map_reduce3_int64_t ) {
	std::cout << "map_reduce3 tests - int64_t\n";
	for( size_t n = 100'000; n >= 100; n /= 10 ) {
		map_reduce_test3<int64_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( scan_int64_t ) {
	std::cout << "scan tests - int64_t\n";
	scan_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		scan_test<int64_t>( n );
	}
}

BOOST_AUTO_TEST_CASE( find_if_int64_t ) {
	std::cout << "find_if tests - int64_t\n";
	find_if_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		find_if_test<int64_t>( n );
	}
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

BOOST_AUTO_TEST_CASE( count_int64_t ) {
	std::cout << "count tests - int64_t\n";
	count_test<int64_t>( LARGE_TEST_SZ );
	for( size_t n = MAX_ITEMS; n >= 100; n /= 10 ) {
		count_test<int64_t>( n );
	}
}

namespace chunked_for_each_001_ns {
	struct call_t {
		template<typename Range>
		constexpr void operator( )( Range rng ) const noexcept {
			auto first = rng.begin( );
			auto const last = rng.end( );
			while( first != last ) {
				( *first++ ) *= 2;
			}
		}
	};
	BOOST_AUTO_TEST_CASE( chunked_for_each_001 ) {
		auto a = daw::make_random_data<int64_t>( 100'000 );
		daw::algorithm::parallel::chunked_for_each(
		  a.data( ), a.data( ) + a.size( ), call_t{} );
		daw::do_not_optimize( a );
	}
} // namespace chunked_for_each_001_ns

BOOST_AUTO_TEST_CASE( chunked_for_each_pos_001 ) {
	auto a = daw::make_random_data<int64_t>( 100'000 );

	std::vector<int64_t> b{};
	b.resize( a.size( ) );
	daw::algorithm::parallel::chunked_for_each_pos(
	  a.cbegin( ), a.cend( ), [&b]( auto rng, size_t start_pos ) mutable {
		  for( size_t n = 0; n < rng.size( ); ++n ) {
			  b[start_pos + n] = rng[n] * 2;
		  }
	  } );
	daw::do_not_optimize( b );
}
