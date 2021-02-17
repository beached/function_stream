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
#include "daw/fs/impl/daw_latch.h"

#include "daw/fs/task_scheduler.h"

using real_t = double;

real_t fib( uintmax_t n ) {
	return n * n;
	/*
	if( n <= 1 ) {
		return static_cast<real_t>( n );
	}
	real_t last = 1;
	real_t result = 1;
	for( uintmax_t m = 2; m < n; ++m ) {
		auto new_last = result;
		result += last;
		last = new_last;
	}
	return result;
	 */
}

void test_task_scheduler( ) {
	constexpr intmax_t const ITEMS = 100u;

	std::cout << "Using " << std::thread::hardware_concurrency( ) << " threads\n";

	auto const nums = [&] {
		auto result = std::vector<uintmax_t>( );
		for( intmax_t n = 0; n < ITEMS; ++n ) {
			result.push_back( daw::randint<uintmax_t>( 500, 9999 ) );
		}
		return result;
	}( );

	auto ts = daw::task_scheduler( ); // get_task_scheduler( );
	ts.start( );
	daw::expecting( ts.started( ) );
	daw::bench_n_test<3>( "parallel", [&]( ) {
		auto mut = std::mutex{ };
		auto sem = daw::shared_latch( std::size( nums ) );
		auto results = std::vector<real_t>( );
		results.reserve( nums.size( ) );
		for( auto i : nums ) {
			(void)ts.add_task( [=, &mut, &results]( ) mutable {
				ts.wait_for_scope( [=, &mut, &results]( ) mutable {
					{
						auto result = fib( i );
						auto const lck = std::unique_lock<std::mutex>( mut );
						results.push_back( result );
					}
					sem.notify( );
				} );
			} );
		}
		sem.wait( );
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
	daw::expecting( 832040U, ans );
}

int main( ) {
	test_task_scheduler( );
	create_waitable_task_test_001( );
}
