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
#include <thread>

#include <daw/daw_benchmark.h>
#include <daw/daw_random.h>
#include <daw/parallel/daw_locked_stack.h>
#include <daw/parallel/daw_semaphore.h>

#include "daw/fs/task_scheduler.h"

using real_t = double;

real_t fib( std::uint64_t n ) {
	if( n <= 1 ) {
		return static_cast<real_t>( n );
	}
	real_t last = 1;
	real_t result = 1;
	for( std::uint64_t m = 2; m < n; ++m ) {
		auto new_last = result;
		result += last;
		last = new_last;
	}
	return result;
}

void test_task_scheduler( ) {
	constexpr std::int64_t const ITEMS = 100u;

	std::cout << "Using " << std::thread::hardware_concurrency( ) << " threads\n";

	auto const nums = [&]( ) {
		auto result = std::vector<std::uint64_t>( );
		for( std::int64_t n = 0; n < ITEMS; ++n ) {
			result.push_back( daw::randint<std::uint64_t>( 500, 9999 ) );
		}
		return result;
	}( );

	auto ts = daw::get_task_scheduler( );
	daw::expecting( ts.started( ) );
	daw::bench_n_test<3>( "parallel", [&]( ) {
		auto results = daw::locked_stack_t<real_t>( );
		auto sem = daw::fixed_cnt_sem( ITEMS );
		for( auto i : nums ) {
			(void)ts.add_task( [&results, &sem, i, &ts]( ) {
				ts.wait_for_scope( [&]( ) { results.push_back( fib( i ) ); } );
				sem.notify( );
			} );
		}
		sem.wait( );
		daw::do_not_optimize( results );
	} );

	daw::bench_n_test<3>( "sequential", [&]( ) {
		auto results = std::vector<real_t>( );
		for( auto i : nums ) {
			results.push_back( fib( i ) );
		}
		daw::do_not_optimize( results );
	} );

	std::cout << "stopping task scheduler\n";
	ts.stop( );
}

void create_waitable_task_test_001( ) {
	real_t ans = 0;
	auto ts = daw::create_waitable_task( [&ans]( ) { ans = fib( 30 ); } );
	ts.wait( );
	if( ans != 832040U ) {
		std::abort( );
	}
}

int main( ) {
	test_task_scheduler( );
	create_waitable_task_test_001( );
}
