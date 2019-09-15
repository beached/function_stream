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

#include <daw/parallel/daw_latch.h>

#include "daw/fs/message_queue.h"

int main( ) {
	//	for( size_t j = 0; j < 25U; ++j ) {
	auto q = ::daw::parallel::locking_circular_buffer<size_t, 512>( );
	std::array<size_t, 10> results{};

	auto l = ::daw::latch( 1 );

	auto const producer = [&]( ) {
		size_t count = 0;
		for( size_t n = 1; n <= 100; ++n ) {
			auto v = n;
			auto tmp_n = n;
			auto r =
			  q.push_back( ::daw::move( tmp_n ), []( auto &&... ) { return true; } );
			++count;
			while( r != ::daw::parallel::push_back_result::success ) {
				++count;
				n = v;
				tmp_n = n;
				r = q.push_back( ::daw::move( tmp_n ), []( auto &&... ) { return true; } );
			}
			l.notify( );
		}
	};
	auto const consumer = [&]( size_t i ) {
		results[i] = 0;
		size_t count = 0;
		l.wait( );
		for( size_t n = 0; n < 100; ++n ) {
			auto val = q.try_pop_front( );
			++count;
			while( not val ) {
				++count;
				val = q.try_pop_front( );
				if( val and val % 2 != 0 ) {
					val *= 2;
					(void)q.push_back( ::daw::move( val ), []( ) { return true; } );
				}
			}
			results[i] += val;
		}
	};

	auto t0 = ::std::thread( producer );
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
	//}
}
