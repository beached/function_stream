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

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <daw/daw_benchmark.h>
#include <daw/daw_random.h>
#include <daw/daw_span.h>

#include "daw/fs/algorithms.h"

/*
template<typename T, typename BinaryFunction>
void k_means_run( daw::span<T> view, daw::span<size_t> labels,
                  daw::span<size_t> centres, BinaryFunction dist_func ) {

  daw::exception::precondition_check(
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
            cur_dist = daw::move( tmp_dist );
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
 */

template<size_t N, typename Iterator, typename Position>
constexpr decltype( auto ) random_value( Iterator first, size_t const &count,
                                         Position &cur_dist,
                                         Position const &max_dist ) {
	auto const range =
	  ( max_dist - cur_dist ) / static_cast<Position>( N - count );
	auto const pos = ::daw::randint<Position>( cur_dist, cur_dist + range );
	std::advance( first, pos );
	cur_dist += pos;
	return *first;
}

template<size_t N, typename Iterator, typename LastType, size_t... Is>
constexpr auto random_values( Iterator first, LastType last,
                              std::index_sequence<Is...> ) {
	auto const max_dist = std::distance( first, last );
	typename std::iterator_traits<Iterator>::difference_type cur_pos = 0;
	return std::array{random_value<N>( first, Is, cur_pos, max_dist )...};
}

template<size_t N, typename Values,
         typename Indices = std::make_index_sequence<N>>
constexpr auto random_values( Values &&values ) {
	using ::std::begin;
	using ::std::end;
	return random_values<N>( begin( values ), end( values ), Indices{} );
}

template<size_t k_num_clusters, typename Values>
auto k_means( Values const &values ) {
	using iterator_t = decltype( std::begin( values ) );
	using value_t = daw::remove_cvref_t<decltype( *std::declval<iterator_t>( ) )>;
	struct result_t {
		std::array<std::vector<iterator_t>, k_num_clusters> clusters;
		std::array<value_t, k_num_clusters> centres;
	};
	result_t result = {{}, random_values<k_num_clusters>( values )};

	return result;
}

int main( int, char ** ) {
	std::cout << "k-means test\n";
	auto points = ::std::vector<std::pair<int64_t, int64_t>>( );
	points.resize( 10'000'000 );
	for( auto &p : points ) {
		p.first = ::daw::randint<int64_t>( -100, 100 );
		p.second = ::daw::randint<int64_t>( -100, 100 );
	}
	auto result = k_means<3>( points );
	::daw::do_not_optimize( result );
	return EXIT_SUCCESS;
}
