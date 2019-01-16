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

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <iostream>
#include <thread>

#include <daw/daw_benchmark.h>
#include <daw/daw_random.h>
#include <daw/parallel/daw_locked_stack.h>
#include <daw/parallel/daw_semaphore.h>

#include "daw/fs/task_scheduler.h"

using real_t =
  boost::multiprecision::number<boost::multiprecision::cpp_dec_float<10000>>;

real_t fib( uintmax_t n ) {
	if( n <= 1 ) {
		return n;
	}
	real_t last = 1;
	real_t result = 1;
	for( uintmax_t m = 2; m < n; ++m ) {
		auto new_last = result;
		result += last;
		last = new_last;
	}
	return result;
}

void test_task_scheduler( ) {
	constexpr size_t const ITEMS = 100u;

	std::cout << "Using " << std::thread::hardware_concurrency( ) << " threads\n";

	auto const nums = []( ) {
		auto result = std::vector<uintmax_t>{};
		for( size_t n = 0; n < ITEMS; ++n ) {
			result.push_back( daw::randint<uintmax_t>( 500, 9999 ) );
		}
		return result;
	}( );

	auto ts = daw::get_task_scheduler( );
	daw::expecting( ts.started( ) );
	auto par_test = [&]( ) {
		auto results = daw::locked_stack_t<real_t>{};
		auto sem = daw::semaphore( 1 - ITEMS );
		for( auto i : nums ) {
			ts.add_task( [&results, &sem, i]( ) {
				results.push_back( fib( i ) );
				sem.notify( );
			} );
		}
		sem.wait( );
		daw::do_not_optimize( results );
	};
	auto seq_test = [&]( ) {
		auto results = std::vector<real_t>{};
		for( auto i : nums ) {
			results.push_back( fib( i ) );
		}
		daw::do_not_optimize( results );
	};

	auto par_t1 = daw::benchmark( par_test );
	auto seq_t1 = daw::benchmark( seq_test );
	auto par_t2 = daw::benchmark( par_test );
	auto seq_t2 = daw::benchmark( seq_test );
	auto par_avg = ( par_t1 + par_t2 ) / 2.0;
	auto seq_avg = ( seq_t1 + seq_t2 ) / 2.0;

	std::cout << "Sequential time: t1-> "
	          << daw::utility::format_seconds( seq_t1, 2 ) << " t2-> "
	          << daw::utility::format_seconds( seq_t2, 2 ) << " average-> "
	          << daw::utility::format_seconds( seq_avg, 2 ) << '\n';

	std::cout << "Parallel time: t1-> "
	          << daw::utility::format_seconds( par_t1, 2 ) << " t2-> "
	          << daw::utility::format_seconds( par_t2, 2 ) << " average-> "
	          << daw::utility::format_seconds( par_avg, 2 ) << '\n';

	std::cout << "diff-> " << ( seq_avg / par_avg ) << '\n';
	ts.stop( );
}

void create_waitable_task_test_001( ) {
	real_t ans = 0;
	auto ts = daw::create_waitable_task( [&ans]( ) { ans = fib( 30 ); } );
	ts.wait( );
	daw::expecting( 832040U, ans );
}

int main( ) {
	test_task_scheduler( );
	create_waitable_task_test_001( );
}
