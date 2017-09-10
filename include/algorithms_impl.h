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

#pragma once

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>

#include <daw/cpp_17.h>
#include <daw/daw_algorithm.h>
#include <daw/daw_semaphore.h>

#include "function_stream.h"
#include "iterator_range.h"
#include "task_scheduler.h"

namespace daw {
	namespace algorithm {
		namespace parallel {
			namespace impl {
				template<size_t MinRangeSize, typename Iterator>
				auto get_part_info( Iterator first, Iterator last, size_t max_parts ) noexcept {
					static_assert( MinRangeSize != 0, "Minimum range size must be > 0" );
					auto const sz = static_cast<size_t>( std::distance( first, last ) );
					struct {
						size_t size;
						size_t count;
					} result;
					result.size = [&]( ) {
						auto r = sz / max_parts;
						if( r < MinRangeSize ) {
							r = MinRangeSize;
						}
						return r;
					}( );
					result.count = sz / result.size;
					if( result.count == 0 )
						if( result.count == 0 ) {
							result.count = 1;
						}
					if( static_cast<size_t>( sz ) > ( result.count * result.size ) ) {
						++result.count;
					}
					return result;
				}

				template<size_t MinRangeSize = 1>
				struct split_range_t {
					static_assert( MinRangeSize != 0, "Minimum range size must be > 0" );
					static constexpr size_t min_range_size = MinRangeSize;

					template<typename Iterator>
					std::vector<iterator_range_t<Iterator>> operator( )( Iterator first, Iterator last,
					                                                     size_t const max_parts ) const {
						std::vector<iterator_range_t<Iterator>> results;
						auto const part_info = get_part_info<min_range_size>( first, last, max_parts );
						auto last_pos =
						    std::exchange( first, daw::algorithm::safe_next( first, last, part_info.size ) );
						while( first != last ) {
							results.push_back( {last_pos, first} );
							last_pos = std::exchange( first, daw::algorithm::safe_next( first, last, part_info.size ) );
						}
						if( std::distance( last_pos, first ) > 0 ) {
							results.push_back( {last_pos, first} );
						}
						return results;
					}
				};

				template<typename Ranges, typename Func>
				daw::shared_semaphore partition_range( Ranges const &ranges, Func func, task_scheduler ts ) {
					daw::shared_semaphore semaphore{1 - static_cast<intmax_t>( ranges.size( ) )};
					for( auto const &rng : ranges ) {
						schedule_task( semaphore, [func, rng]( ) mutable { func( rng ); }, ts );
					}
					return semaphore;
				}

				template<typename Ranges, typename Func>
				daw::shared_semaphore partition_range_pos( Ranges const &ranges, Func func, task_scheduler ts ) {
					daw::shared_semaphore semaphore{1 - static_cast<intmax_t>( ranges.size( ) )};
					for( size_t n = 0; n < ranges.size( ); ++n ) {
						schedule_task( semaphore, [ func, rng = ranges[n], n ]( ) mutable { func( rng, n ); }, ts );
					}
					return semaphore;
				}

				template<typename PartitionPolicy, typename Iterator, typename Func>
				daw::shared_semaphore partition_range( Iterator first, Iterator const last, Func func,
				                                       task_scheduler ts ) {
					if( std::distance( first, last ) == 0 ) {
						return daw::shared_semaphore{1};
					}
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					daw::shared_semaphore semaphore{1 - static_cast<intmax_t>( ranges.size( ) )};
					for( auto const &rng : ranges ) {
						schedule_task( semaphore, [func, rng]( ) { func( rng.first, rng.last ); }, ts );
					}
					return semaphore;
				}

				template<typename PartitionPolicy = split_range_t<1>, typename RandomIterator, typename Func>
				void parallel_for_each( RandomIterator first, RandomIterator last, Func func, task_scheduler ts ) {
					partition_range<PartitionPolicy>( first, last,
					                                  [func]( RandomIterator f, RandomIterator l ) {
						                                  for( auto it = f; it != l; ++it ) {
							                                  func( *it );
						                                  }
					                                  },
					                                  ts )
					    .wait( );
				}

				template<typename Ranges, typename Func>
				void parallel_for_each( Ranges &ranges, Func func, task_scheduler ts ) {
					partition_range( ranges,
					                 [func]( auto f, auto l ) {
						                 for( auto it = f; it != l; ++it ) {
							                 func( *it );
						                 }
					                 },
					                 ts )
					    .wait( );
				}

				template<typename Iterator, typename Function>
				void merge_reduce_range( std::vector<iterator_range_t<Iterator>> ranges, Function func,
				                         task_scheduler ts ) {
					while( ranges.size( ) > 1 ) {
						auto const count = ( ranges.size( ) % 2 == 0 ? ranges.size( ) : ranges.size( ) - 1 );
						std::vector<iterator_range_t<Iterator>> next_ranges;
						next_ranges.reserve( count );
						daw::semaphore semaphore{1 - static_cast<size_t>( count ) / 2};
						for( size_t n = 1; n < count; n += 2 ) {
							next_ranges.push_back( {ranges[n - 1].first, ranges[n].last} );
						}
						if( count != ranges.size( ) ) {
							next_ranges.push_back( ranges.back( ) );
						}
						for( size_t n = 1; n < count; n += 2 ) {
							ts.add_task( [func, &ranges, n, &semaphore]( ) {
								func( ranges[n - 1].first, ranges[n].first, ranges[n].last );
								semaphore.notify( );
							} );
						}
						semaphore.wait( );
						std::swap( ranges, next_ranges );
					}
				}

				template<typename PartitionPolicy = split_range_t<512>, typename Iterator, typename Sort,
				         typename Compare>
				void parallel_sort( Iterator first, Iterator last, Sort sort, Compare compare, task_scheduler ts ) {
					if( PartitionPolicy::min_range_size > static_cast<size_t>( std::distance( first, last ) ) ) {
						sort( first, last, compare );
						return;
					}
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					partition_range(
					    ranges, [sort, compare]( auto const &range ) { sort( range.begin( ), range.end( ), compare ); },
					    ts )
					    .wait( );

					merge_reduce_range(
					    ranges,
					    [compare]( Iterator f, Iterator m, Iterator l ) { std::inplace_merge( f, m, l, compare ); },
					    ts );
				}

				template<typename PartitionPolicy = split_range_t<1>, typename T, typename Iterator, typename BinaryOp>
				auto parallel_reduce( Iterator first, Iterator last, T init, BinaryOp binary_op, task_scheduler ts ) {
					using result_t = std::decay_t<decltype( binary_op( init, *first ) )>;
					{
						auto const sz = std::distance( first, last );
						if( sz < 2 ) {
							if( sz == 0 ) {
								return static_cast<result_t>( init );
							}
							return binary_op( init, *first );
						}
					}
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<std::unique_ptr<T>> results{ranges.size( )};
					auto semaphore =
					    partition_range_pos( ranges,
					                         [&results, binary_op]( iterator_range_t<Iterator> range, size_t n ) {
						                         auto result =
						                             binary_op( range.front( ), *std::next( range.begin( ) ) );
						                         range.advance( 2 );
						                         while( !range.empty( ) ) {
							                         result = binary_op( result, range.pop_front( ) );
						                         }
						                         results[n] = std::make_unique<T>( std::move( result ) );
					                         },
					                         ts );
					ts.blocking_on_waitable( semaphore );
					auto result = static_cast<result_t>( init );
					for( size_t n = 0; n < ranges.size( ); ++n ) {
						result = binary_op( result, *results[n] );
					}
					return result;
				}

				template<typename Type1, typename Type2, typename Compare>
				Type1 compare_value( Type1 const &lhs, Type2 const &rhs, Compare cmp ) {
					if( cmp( lhs, rhs ) ) {
						return lhs;
					}
					return rhs;
				}

				template<typename PartitionPolicy = split_range_t<2>, typename Iterator, typename Compare>
				Iterator parallel_min_element( Iterator const first, Iterator const last, Compare cmp,
				                               task_scheduler ts ) {
					if( first == last ) {
						return last;
					}
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<Iterator> results{ranges.size( )};
					auto semaphore = partition_range_pos(
					    ranges,
					    [&results, cmp]( iterator_range_t<Iterator> range, size_t n ) {
						    results[n] = std::min_element(
						        range.cbegin( ), range.cend( ),
						        [cmp]( auto const &lhs, auto const &rhs ) { return cmp( lhs, rhs ); } );
					    },
					    ts );
					ts.blocking_on_waitable( semaphore );
					return *std::min_element( results.cbegin( ), results.cend( ),
					                          [cmp]( auto const &lhs, auto const &rhs ) { return cmp( *lhs, *rhs ); } );
				}

				template<typename PartitionPolicy = split_range_t<2>, typename Iterator, typename Compare>
				Iterator parallel_max_element( Iterator const first, Iterator const last, Compare cmp,
				                               task_scheduler ts ) {
					if( first == last ) {
						return last;
					}
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<Iterator> results{ranges.size( )};
					auto semaphore = partition_range_pos(
					    ranges,
					    [&results, cmp]( iterator_range_t<Iterator> range, size_t n ) {
						    results[n] = std::max_element(
						        range.cbegin( ), range.cend( ),
						        [cmp]( auto const &lhs, auto const &rhs ) { return cmp( lhs, rhs ); } );
					    },
					    ts );
					ts.blocking_on_waitable( semaphore );
					return *std::max_element( results.cbegin( ), results.cend( ),
					                          [cmp]( auto const &lhs, auto const &rhs ) { return cmp( *lhs, *rhs ); } );
				}

				template<typename PartitionPolicy = split_range_t<512>, typename Iterator, typename OutputIterator,
				         typename UnaryOperation>
				void parallel_map( Iterator first1, Iterator const last1, OutputIterator first2,
				                   UnaryOperation unary_op, task_scheduler ts ) {

					partition_range<PartitionPolicy>( first1, last1,
					                                  [first1, first2, unary_op]( Iterator f, Iterator const l ) {
						                                  auto out_it = std::next( first2, std::distance( first1, f ) );

						                                  for( ; f != l; ++f, ++out_it ) {
							                                  *out_it = unary_op( *f );
						                                  }
					                                  },
					                                  ts )
					    .wait( );
				}

				template<typename PartitionPolicy = split_range_t<512>, typename Iterator, typename T,
				         typename MapFunction, typename ReduceFunction>
				auto parallel_map_reduce( Iterator first, Iterator last, T const &init, MapFunction map_function,
				                          ReduceFunction reduce_function, task_scheduler ts ) {
					static_assert( PartitionPolicy::min_range_size >= 2, "Minimum range size must be >= 2" );
					daw::exception::daw_throw_on_false( std::distance( first, last ) >= 2,
					                                    "Must be at least 2 items in range" );
					using result_t = std::decay_t<decltype( reduce_function(
					    map_function( *std::declval<Iterator>( ) ), map_function( *std::declval<Iterator>( ) ) ) )>;

					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<std::unique_ptr<result_t>> results{ranges.size( )};

					auto semaphore = partition_range_pos(
					    ranges,
					    [&results, map_function, reduce_function]( iterator_range_t<Iterator> range, size_t n ) {
						    result_t result = map_function( range.pop_front( ) );
						    while( !range.empty( ) ) {
							    result = reduce_function( result, map_function( range.pop_front( ) ) );
						    }
						    results[n] = std::make_unique<result_t>( std::move( result ) );
					    },
					    ts );

					ts.blocking_on_waitable( semaphore );
					auto result = reduce_function( map_function( init ), *results[0] );
					for( size_t n = 1; n < ranges.size( ); ++n ) {
						result = reduce_function( result, *results[n] );
					}
					return result;
				}

				template<typename PartitionPolicy = split_range_t<1>, typename Iterator, typename OutputIterator,
				         typename BinaryOp>
				void parallel_scan( Iterator first, Iterator last, OutputIterator first_out, OutputIterator last_out,
				                    BinaryOp binary_op, task_scheduler ts ) {
					{
						auto const sz = static_cast<size_t>( std::distance( first, last ) );
						daw::exception::daw_throw_on_false(
						    sz == static_cast<size_t>( std::distance( first_out, last_out ) ),
						    "Output range must be the same size as input" );
						if( sz == 2 ) {
							*first_out = *first;
							return;
						}
					}
					using value_t = std::decay_t<decltype( binary_op( *first, *first ) )>;

					auto ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<std::unique_ptr<value_t>> p1_results;
					p1_results.resize( ranges.size( ) );
					// Sum each sub range
					auto semaphore = partition_range_pos(
					    ranges,
					    [&p1_results, binary_op, first_out]( iterator_range_t<Iterator> rng, size_t n ) {
						    if( n == 0 ) {
							    auto const lst = std::partial_sum( rng.cbegin( ), rng.cend( ), first_out, binary_op );
							    p1_results[n] = std::make_unique<value_t>(
							        *std::next( first_out, std::distance( first_out, lst ) - 1 ) );
							    return;
						    }
						    value_t result = rng.pop_front( );
						    auto const rend = rng.cend( );
						    for( auto it = rng.cbegin( ); it != rend; ++it ) {
							    result = binary_op( result, *it );
						    }
						    p1_results[n] = std::make_unique<value_t>( std::move( result ) );
					    },
					    ts );

					ts.blocking_on_waitable( semaphore );

					std::vector<value_t> range_sums;
					range_sums.resize( p1_results.size( ) );
					auto const ressize = p1_results.size( );
					for( size_t n = 1; n < ressize; ++n ) {
						range_sums[n] = *p1_results[0];
						for( size_t m = 1; m < n; ++m ) {
							range_sums[n] = binary_op( range_sums[n], *p1_results[m] );
						}
					}

					semaphore = partition_range_pos(
					    ranges,
					    [&range_sums, first, first_out, binary_op]( iterator_range_t<Iterator> cur_range, size_t n ) {
						    if( n == 0 ) {
							    return;
						    }
						    auto out_pos = std::next( first_out, std::distance( first, cur_range.begin( ) ) );
						    auto const &cur_value = range_sums[n];
						    auto sum = binary_op( cur_value, cur_range.pop_front( ) );
						    *( out_pos++ ) = sum;
						    while( !cur_range.empty( ) ) {
							    sum = binary_op( sum, cur_range.pop_front( ) );
							    *out_pos = sum;
							    ++out_pos;
						    }
					    },
					    ts );
					ts.blocking_on_waitable( semaphore );
				}
			} // namespace impl
		}     // namespace parallel
	}         // namespace algorithm
} // namespace daw
