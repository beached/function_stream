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

#include <array>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#include <daw/fs/impl/daw_latch.h>

#include "daw/fs/message_queue.h"

int main( ) {
	//	for( size_t j = 0; j < 25U; ++j ) {
	auto q = daw::parallel::concurrent_queue<size_t>( );
	std::array<size_t, 100> results{ };
	auto mut_out = std::mutex( );

	auto l = daw::fixed_cnt_sem( 1 );
	auto l2 = daw::fixed_cnt_sem( 1 );

	auto const producer = [&]( ) {
		size_t count = 0;
		(void)count;
		for( size_t n = 1; n <= 100; ++n ) {
			auto v = n;
			auto tmp_n = n;
			auto r = push_back( q, std::move( tmp_n ), []( auto &&... ) { return true; } );
			++count;
			while( r != daw::parallel::push_back_result::success ) {
				++count;
				n = v;
				tmp_n = n;
				r = push_back( q, std::move( tmp_n ), []( auto &&... ) { return true; } );
			}
			// l is a one-shot latch (count 1): open it once the first value is queued
			if( n == 1 ) {
				l.notify( );
			}
		}
		l2.notify( );
		auto const lck = std::lock_guard( mut_out );
		std::cout << "producer - done\n";
	};
	auto const consumer = [&]( size_t i ) {
		results[i] = 0;
		l.wait( );
		// Drain until the producer has finished and the queue is empty.
		// try_pop_front can fail spuriously under lock contention, so only stop
		// once the producer is done (l2) and the queue reports empty.
		while( true ) {
			if( auto val = q.try_pop_front( ); val ) {
				results[i] += *val;
				continue;
			}
			if( l2.try_wait( ) and q.empty( ) ) {
				return;
			}
			std::this_thread::yield( );
		}
	};

	auto t0 = std::thread( producer );
	auto consumers = std::vector<std::thread>( );
	for( size_t n = 0; n < results.size( ); ++n ) {
		consumers.emplace_back( consumer, n );
	}
	t0.join( );
	for( auto &c : consumers ) {
		c.join( );
	}
	auto const result = std::accumulate( begin( results ), end( results ), 0ULL );
	std::cout << result << '\n';
	// The producer pushes 1..100 exactly once, so every value must be consumed once
	if( result != 5050ULL ) {
		std::cerr << "Expected 5050, got " << result << '\n';
		return 1;
	}
	//}
}
