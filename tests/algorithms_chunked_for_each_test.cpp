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

#define BOOST_TEST_MODULE parallel_algorithms_chunked_for_each
#include <daw/boost_test.h>

#include "algorithms.h"

#include "common.h"

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
