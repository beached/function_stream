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
#include <daw/daw_spin_lock.h>

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
						auto last_pos = std::exchange( first, daw::algorithm::safe_next( first, last, part_info.size ) );
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
					daw::shared_semaphore sem{1 - static_cast<intmax_t>( ranges.size( ) )};
					for( auto const &rng : ranges ) {
						schedule_task( sem, [func, rng]( ) mutable { func( rng ); }, ts );
					}
					return sem;
				}

				template<typename Ranges, typename Func>
				daw::shared_semaphore partition_range_pos( Ranges const &ranges, Func func, task_scheduler ts,
				                                           size_t const start_pos = 0 ) {
					daw::shared_semaphore sem{1 - static_cast<intmax_t>( ranges.size( ) - start_pos )};
					for( size_t n = start_pos; n < ranges.size( ); ++n ) {
						schedule_task( sem, [ func, rng = ranges[n], n ]( ) mutable { func( rng, n ); }, ts );
					}
					return sem;
				}

				template<typename PartitionPolicy, typename Iterator, typename Func>
				daw::shared_semaphore partition_range( Iterator first, Iterator const last, Func func, task_scheduler ts ) {
					if( std::distance( first, last ) == 0 ) {
						return daw::shared_semaphore{1};
					}
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					daw::shared_semaphore sem{1 - static_cast<intmax_t>( ranges.size( ) )};
					for( auto const &rng : ranges ) {
						schedule_task( sem, [func, rng]( ) { func( rng.first, rng.last ); }, ts );
					}
					return sem;
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

				template<typename PartitionPolicy = split_range_t<1>, typename RandomIterator, typename Func>
				void parallel_for_each_index( RandomIterator first, RandomIterator last, Func func, task_scheduler ts ) {
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					daw::shared_semaphore sem{1 - static_cast<intmax_t>( ranges.size( ) )};
					partition_range( ranges,
					                 [func, first]( auto f, auto l ) {
						                 auto const start_pos = static_cast<size_t>( first, f );
						                 auto const end_pos = static_cast<size_t>( first, l );
						                 for( size_t n = start_pos; n < end_pos; ++n ) {
							                 func( n );
						                 }

					                 },
					                 ts )
					  .wait( );
				}

				template<typename Iterator, typename Function>
				void merge_reduce_range( std::vector<iterator_range_t<Iterator>> ranges, Function func, task_scheduler ts ) {
					while( ranges.size( ) > 1 ) {
						auto const count = ( ranges.size( ) % 2 == 0 ? ranges.size( ) : ranges.size( ) - 1 );
						std::vector<iterator_range_t<Iterator>> next_ranges;
						next_ranges.reserve( count );
						daw::semaphore sem{1 - static_cast<size_t>( count ) / 2};
						for( size_t n = 1; n < count; n += 2 ) {
							next_ranges.push_back( {ranges[n - 1].first, ranges[n].last} );
						}
						if( count != ranges.size( ) ) {
							next_ranges.push_back( ranges.back( ) );
						}
						for( size_t n = 1; n < count; n += 2 ) {
							ts.add_task( [func, &ranges, n, &sem]( ) {
								func( ranges[n - 1].first, ranges[n].first, ranges[n].last );
								sem.notify( );
							} );
						}
						sem.wait( );
						std::swap( ranges, next_ranges );
					}
				}

				template<typename PartitionPolicy = split_range_t<512>, typename Iterator, typename Sort, typename Compare>
				void parallel_sort( Iterator first, Iterator last, Sort sort, Compare compare, task_scheduler ts ) {
					if( PartitionPolicy::min_range_size > static_cast<size_t>( std::distance( first, last ) ) ) {
						sort( first, last, compare );
						return;
					}
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					partition_range( ranges,
					                 [sort, compare]( auto const &range ) { sort( range.begin( ), range.end( ), compare ); }, ts )
					  .wait( );

					merge_reduce_range(
					  ranges, [compare]( Iterator f, Iterator m, Iterator l ) { std::inplace_merge( f, m, l, compare ); }, ts );
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
					auto sem =
					  partition_range_pos( ranges,
					                       [&results, binary_op]( iterator_range_t<Iterator> range, size_t n ) {
						                       results[n] = std::make_unique<T>( std::accumulate(
						                         std::next( range.cbegin( ) ), range.cend( ), range.front( ), binary_op ) );
					                       },
					                       ts );
					ts.wait_for( sem );
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
				Iterator parallel_min_element( Iterator const first, Iterator const last, Compare cmp, task_scheduler ts ) {
					if( first == last ) {
						return last;
					}
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<Iterator> results{ranges.size( )};
					auto sem = partition_range_pos( ranges,
					                                [&results, cmp]( iterator_range_t<Iterator> range, size_t n ) {
						                                results[n] = std::min_element(
						                                  range.cbegin( ), range.cend( ),
						                                  [cmp]( auto const &lhs, auto const &rhs ) { return cmp( lhs, rhs ); } );
					                                },
					                                ts );
					ts.wait_for( sem );
					return *std::min_element( results.cbegin( ), results.cend( ),
					                          [cmp]( auto const &lhs, auto const &rhs ) { return cmp( *lhs, *rhs ); } );
				}

				template<typename PartitionPolicy = split_range_t<2>, typename Iterator, typename Compare>
				Iterator parallel_max_element( Iterator const first, Iterator const last, Compare cmp, task_scheduler ts ) {
					if( first == last ) {
						return last;
					}
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<Iterator> results{ranges.size( )};
					auto sem = partition_range_pos( ranges,
					                                [&results, cmp]( iterator_range_t<Iterator> range, size_t n ) {
						                                results[n] = std::max_element(
						                                  range.cbegin( ), range.cend( ),
						                                  [cmp]( auto const &lhs, auto const &rhs ) { return cmp( lhs, rhs ); } );
					                                },
					                                ts );
					ts.wait_for( sem );
					return *std::max_element( results.cbegin( ), results.cend( ),
					                          [cmp]( auto const &lhs, auto const &rhs ) { return cmp( *lhs, *rhs ); } );
				}

				template<typename PartitionPolicy = split_range_t<512>, typename Iterator, typename OutputIterator,
				         typename UnaryOperation>
				void parallel_map( Iterator first_in, Iterator const last_in, OutputIterator first_out, UnaryOperation unary_op,
				                   task_scheduler ts ) {

					partition_range<PartitionPolicy>( first_in, last_in,
					                                  [first_in, first_out, unary_op]( Iterator first, Iterator const last ) {
						                                  auto out_it = std::next( first_out, std::distance( first_in, first ) );

						                                  for( ; first != last; ++first, ++out_it ) {
							                                  *out_it = unary_op( *first );
						                                  }
					                                  },
					                                  ts )
					  .wait( );
				}

				template<typename PartitionPolicy = split_range_t<512>, typename Iterator, typename T, typename MapFunction,
				         typename ReduceFunction>
				auto parallel_map_reduce( Iterator first, Iterator last, T const &init, MapFunction map_function,
				                          ReduceFunction reduce_function, task_scheduler ts ) {
					static_assert( PartitionPolicy::min_range_size >= 2, "Minimum range size must be >= 2" );
					daw::exception::daw_throw_on_false( std::distance( first, last ) >= 2, "Must be at least 2 items in range" );
					using result_t = std::decay_t<decltype( reduce_function( map_function( *std::declval<Iterator>( ) ),
					                                                         map_function( *std::declval<Iterator>( ) ) ) )>;

					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<std::unique_ptr<result_t>> results{ranges.size( )};

					auto sem = partition_range_pos(
					  ranges,
					  [&results, map_function, reduce_function]( iterator_range_t<Iterator> range, size_t n ) {
						  result_t result = map_function( range.pop_front( ) );
						  while( !range.empty( ) ) {
							  result = reduce_function( result, map_function( range.pop_front( ) ) );
						  }
						  results[n] = std::make_unique<result_t>( std::move( result ) );
					  },
					  ts );

					ts.wait_for( sem );
					auto result = reduce_function( map_function( init ), *results[0] );
					for( size_t n = 1; n < ranges.size( ); ++n ) {
						result = reduce_function( result, *results[n] );
					}
					return result;
				}

				template<typename PartitionPolicy = split_range_t<1024>, typename Iterator, typename OutputIterator,
				         typename BinaryOp>
				void parallel_scan( Iterator first, Iterator last, OutputIterator first_out, OutputIterator last_out,
				                    BinaryOp binary_op, task_scheduler ts ) {
					{
						auto const sz = static_cast<size_t>( std::distance( first, last ) );
						daw::exception::daw_throw_on_false( sz == static_cast<size_t>( std::distance( first_out, last_out ) ),
						                                    "Output range must be the same size as input" );
						if( sz == 2 ) {
							*first_out = *first;
							return;
						}
					}
					using value_t = std::decay_t<decltype( binary_op( *first, *first ) )>;

					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<std::unique_ptr<value_t>> p1_results{ranges.size( )};
					std::vector<daw::spin_lock> mut_p1_results{ranges.size( )};
					auto const add_result = [&]( size_t pos, value_t const &value ) {
						for( size_t n = pos + 1; n < p1_results.size( ); ++n ) {
							std::lock_guard<daw::spin_lock> lck{mut_p1_results[n]};
							if( p1_results[n] ) {
								*p1_results[n] = binary_op( *p1_results[n], value );
							} else {
								p1_results[n] = std::make_unique<value_t>( value );
							}
						}
					};
					// Sum each sub range, but complete the first output as it does
					// not have to be scanned twice
					ts.wait_for( partition_range_pos(
					  ranges,
					  [binary_op, first_out, add_result]( iterator_range_t<Iterator> rng, size_t n ) {
						  if( n == 0 ) {
							  auto const lst = std::partial_sum( rng.cbegin( ), rng.cend( ), first_out, binary_op );
							  add_result( n, *std::next( first_out, std::distance( first_out, lst ) - 1 ) );
							  return;
						  }
						  value_t result = rng.pop_front( );
						  auto const rend = rng.cend( );
						  for( auto it = rng.cbegin( ); it != rend; ++it ) {
							  result = binary_op( result, *it );
						  }
						  add_result( n, result );
					  },
					  ts ) );

					ts.wait_for( partition_range_pos(
					  ranges,
					  [&p1_results, first, first_out, binary_op]( iterator_range_t<Iterator> cur_range, size_t n ) {

						  auto out_pos = std::next( first_out, std::distance( first, cur_range.begin( ) ) );
						  auto const &cur_value = *p1_results[n];
						  auto sum = binary_op( cur_value, cur_range.pop_front( ) );
						  *( out_pos++ ) = sum;
						  while( !cur_range.empty( ) ) {
							  sum = binary_op( sum, cur_range.pop_front( ) );
							  *out_pos = sum;
							  ++out_pos;
						  }
					  },
					  ts, 1 ) );
				}

				template<typename PartitionPolicy = split_range_t<1>, typename Iterator, typename UnaryPredicate>
				Iterator parallel_find_if( Iterator first, Iterator last, UnaryPredicate pred, task_scheduler ts ) {
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<std::unique_ptr<Iterator>> results{ranges.size( )};

					std::atomic<size_t> has_found{std::numeric_limits<size_t>::max( )};
					daw::spin_lock mut_found;
					ts.wait_for( partition_range_pos(
					  ranges,
					  [&results, pred, &has_found, &mut_found]( iterator_range_t<Iterator> range, size_t pos ) {

						  size_t const stride = std::thread::hardware_concurrency( ) * 100;
						  size_t m = 0;
						  for( size_t n = 0; n < range.size( ); n += stride ) {
							  auto const last_val = ( n + stride < range.size( ) ) ? n + stride : range.size( );
							  for( m = n; m < last_val; ++m ) {
								  if( pred( range[m] ) ) {
									  results[pos] = std::make_unique<Iterator>( std::next(
									    range.begin( ), static_cast<typename std::iterator_traits<Iterator>::difference_type>( m ) ) );
									  std::lock_guard<daw::spin_lock> has_found_lck{mut_found};
									  // TODO: why is this atomic?
									  if( pos < has_found.load( std::memory_order_relaxed ) ) {
										  has_found.store( pos, std::memory_order_relaxed );
									  }
									  return;
								  }
							  }
							  if( pos > has_found.load( std::memory_order_relaxed ) ) {
								  return;
							  }
						  }
					  },
					  ts ) );

					for( auto const &it : results ) {
						if( it ) {
							return *it;
						}
					}
					return last;
				}

				template<typename PartitionPolicy = split_range_t<1>, typename Iterator1, typename Iterator2,
				         typename BinaryPredicate>
				bool parallel_equal( Iterator1 first1, Iterator1 last1, Iterator2 first2, Iterator2 last2, BinaryPredicate pred,
				                     task_scheduler ts ) {
					if( std::distance( first1, last1 ) != std::distance( first2, last2 ) ) {
						return false;
					}
					auto const ranges1 = PartitionPolicy{}( first1, last1, ts.size( ) );
					auto const ranges2 = PartitionPolicy{}( first2, last2, ts.size( ) );
					std::atomic_bool is_equal{true};
					ts.wait_for( partition_range_pos(
					  ranges1,
					  [pred, &is_equal, &ranges2]( iterator_range_t<Iterator1> range1, size_t pos ) {
						  auto range2 = ranges2[pos];

						  size_t const stride = std::max( static_cast<size_t>( std::thread::hardware_concurrency( ) * 100 ),
						                                  static_cast<size_t>( PartitionPolicy::min_range_size ) );
						  size_t m = 0;
						  for( size_t n = 0; n < range1.size( ) && is_equal.load( std::memory_order_relaxed ); n += stride ) {
							  auto const last_val = [&]( ) {
								  auto const lv_res = n + stride;
								  if( lv_res > range1.size( ) ) {
									  return range1.size( );
								  }
								  return lv_res;
							  }( );
							  for( m = n; m < last_val; ++m ) {
								  if( !pred( range1[m], range2[m] ) ) {
									  is_equal.store( false, std::memory_order_relaxed );
									  return;
								  }
							  }
						  }
					  },
					  ts ) );
					return is_equal.load( std::memory_order_relaxed );
				}

				template<typename PartitionPolicy = split_range_t<500>, typename RandomIterator, typename UnaryPredicate>
				auto parallel_count( RandomIterator first, RandomIterator last, UnaryPredicate pred, task_scheduler ts ) {
					static_assert( PartitionPolicy::min_range_size >= 2, "Minimum range size must be >= 2" );
					daw::exception::daw_throw_on_false( std::distance( first, last ) >= 2, "Must be at least 2 items in range" );

					using result_t = typename std::iterator_traits<RandomIterator>::difference_type;
					if( static_cast<size_t>( std::distance( first, last ) ) < PartitionPolicy::min_range_size ) {
						return std::count_if( first, last, pred );
					}
					auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
					std::vector<result_t> results( ranges.size( ), 0 );

					auto sem = partition_range_pos( ranges,
					                                [&results, pred]( iterator_range_t<RandomIterator> range, size_t n ) {
						                                results[n] = std::count_if( range.cbegin( ), range.cend( ), pred );
					                                },
					                                ts );

					ts.wait_for( sem );
					return std::accumulate( results.cbegin( ), results.cend( ), static_cast<result_t>( 0 ) );
				}
			} // namespace impl
		}   // namespace parallel
	}     // namespace algorithm
} // namespace daw
