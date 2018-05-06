// The MIT License (MIT)
//
// Copyright (c) 2017 Darrell Wright
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

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <daw/daw_array_view.h>
#include <daw/daw_random.h>

#include "algorithm.h"

template<typename T, typename BinaryFunction>
void k_means_run( daw::array_view<T> view, daw::span<size_t> labels,
                  daw::span<size_t> centres, BinaryFunction dist_func ) {

	daw::exception::daw_throw_on_false(
	  view.size( ) != labels.size( ),
	  "Size mismatch between size of label's span and items view" );
	using iterator = decltype( centres.begin( ) );
	auto ts = daw::get_task_scheduler( );
	// Find closest cluster centre and associate item with it
	using daw::algorithm::parallel::chunked_for_each_pos;

	chunked_for_each_pos(
	  labels.begin( ), labels.end( ),
	  [view, centres, dist_func]( auto range, size_t start_pos ) {
		  auto const sz = range.size( );
		  // Iterator over all labells in this range
		  auto view_range = view.subset( start_pos, range.size( ) );
		  for( size_t n = 0; n < sz; ++n ) {
			  auto cur_cent = 0;
			  auto cur_dist = dist_func( view_range[n], view[centres[0]] );
			  for( size_t m = 1; m < centres.size( ); ++m ) {
				  if( auto tmp_dist =
				        dist_func( view[range[n]], view[centres[m]] ) < cur_dist ) {
					  cur_cent = m;
					  cur_dist = std::move( tmp_dist );
				  }
			  }
			  range[n] = m;
		  }
	  } );
	// Find centre of each cluster

	chunked_for_each_pos(
	  centres.begin( ), centres.end( ),
	  [view, labels, dist_func]( auto range, size_t start_pos ) {
		  auto const sz = range.size( );
	  } );
}

int main( int, char ** ) {
	std::vector<std::pair<int64_t, int64_t>> points;
	points.reserve( 10'000'000 );
	fill_random( std::back_inserter( points ), 10'000'000 );

	return EXIT_SUCCESS;
}
