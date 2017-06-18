// The MIT License (MIT)
//
// Copyright (c) 2016-2017 Darrell Wright
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

#include <algorithm>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <iostream>
#include <random>
#include <thread>

#include <daw/daw_benchmark.h>
#include <daw/daw_locked_stack.h>
#include <daw/daw_traits.h>

#include "future_result.h"
#include "task_scheduler.h"

using real_t = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<10000>>;

constexpr static const auto MIN_PAR_SIZE = 64000;

template<typename IterFirst, typename IterLast, typename TaskScheduler>
daw::future_result_t<void> parallel_reverse( TaskScheduler &ts, IterFirst first, IterLast last );

template<typename IterFirst, typename IterLast, typename TaskScheduler>
daw::future_result_t<void> parallel_reverse( TaskScheduler &ts, IterFirst first, IterLast last ) {
	auto dist = std::distance( first, last );
	if( dist < MIN_PAR_SIZE ) {
		std::reverse( first, last );
		daw::future_result_t<void> result;
		result.set_value( );
		return result;
	}
	if( dist % 2 == 1 ) {
		--dist;
	}
	dist /= 2;
	std::vector<daw::future_result_t<void>> results;
	size_t last_n = 0;
	for( size_t n=dist/ts.size( ); n<dist; dist += dist / ts.size( )) {
		auto func = [&ts, first, last, n, last_n]( ) -> void {
			auto const sz = std::distance( first, last );
			for( size_t m=last_n; m<n; ++m ) {
				using std::swap;
				auto &item_a = *std::next( first, m );
				auto &item_b = *std::next( first, ( sz - m ) - 1 );
				swap( item_a, item_b );
			}
		};
		results.push_back( daw::make_future_result( ts, func ));
	}
	return daw::make_future_result( ts, [results=std::move(results)]( ) {
		for( auto & r: results ) {
			r.wait( );
		}
	});
}

int main( int argc, char **argv ) {
	auto const ITEMS = [argc, argv]( ) -> size_t {
		if( argc < 2 ) {
			return ( MIN_PAR_SIZE / sizeof( uintmax_t ) ) * 1000;
		}
		return strtoull( argv[1], nullptr, 10 );
	}( );

	std::cout << "Using " << std::thread::hardware_concurrency( ) << " threads\n";
	std::random_device rd;
	std::mt19937 gen{rd( )};
	std::uniform_int_distribution<uintmax_t> dis{0, std::numeric_limits<uintmax_t>::max( )};

	daw::task_scheduler ts{};
	auto const data = [&]( ) {
		std::vector<uintmax_t> results;
		results.reserve( ITEMS );
		for( size_t n = 0; n < ITEMS; ++n ) {
			results.emplace_back( dis( gen ) );
		}
		return results;
	}( );
	std::vector<uintmax_t> data2{data.begin( ), data.end( )};

	ts.start( );
	auto const time = daw::benchmark( [&]( ) {
		auto first = data2.begin( );
		auto last = data2.end( );
		auto f = parallel_reverse( ts, first, last );
		f.wait( );
	} );
	std::cout << "For " << ITEMS << "items it took " << time << ": " << '\n';
	return EXIT_SUCCESS;
}

