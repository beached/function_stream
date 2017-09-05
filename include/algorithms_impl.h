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

				template<typename Iterator, size_t MinRangeSize = 1>
				struct split_range_t {
					std::vector<iterator_range_t<Iterator>> operator( )( Iterator first, Iterator last,
					                                                     size_t const max_parts ) const {
						std::vector<iterator_range_t<Iterator>> results;
						auto const part_info = get_part_info<MinRangeSize>( first, last, max_parts );
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

				template<size_t MinRangeSize = 1, typename Iterator, typename Func>
				auto partition_range( Iterator first, Iterator const last, Func func ) {
					if( std::distance( first, last ) == 0 ) {
						return daw::shared_semaphore{1};
					}
					auto ts = get_task_scheduler( );
					auto const ranges = split_range_t<Iterator, MinRangeSize>{}( first, last, ts.size( ) );
					daw::shared_semaphore semaphore{1 - static_cast<intmax_t>( ranges.size( ) )};
					for( auto const &rng : ranges ) {
						schedule_task( semaphore, [func, rng]( ) { func( rng.first, rng.last ); }, ts );
					}
					return semaphore;
				}

				template<typename RandomIterator, typename Func>
				void parallel_for_each( RandomIterator const first, RandomIterator const last, Func func ) {
					impl::partition_range( first, last,
					                       [func]( RandomIterator f, RandomIterator l ) {
						                       for( auto it = f; it != l; ++it ) {
							                       func( *it );
						                       }
					                       } )
					    .wait( );
				}

				template<typename Iterator, typename Function>
				void merge_reduce_range( std::vector<std::pair<Iterator, Iterator>> ranges, Function func ) {
					auto ts = get_task_scheduler( );
					while( ranges.size( ) > 1 ) {
						std::vector<std::pair<Iterator, Iterator>> next_ranges;
						auto const count = ( ranges.size( ) % 2 == 0 ? ranges.size( ) : ranges.size( ) - 1 );
						daw::shared_semaphore semaphore{1 - ( static_cast<intmax_t>( count ) / 2 )};
						for( size_t n = 1; n < count; n += 2 ) {
							daw::exception::daw_throw_on_false( ranges[n - 1].second == ranges[n].first,
							                                    "Non continuous range" );
							schedule_task( semaphore,
							               [func, &ranges, n]( ) mutable {
								               func( ranges[n - 1].first, ranges[n].first, ranges[n].second );
							               },
							               ts );

							next_ranges.push_back( std::make_pair( ranges[n - 1].first, ranges[n].second ) );
						}
						semaphore.wait( );
						if( count != ranges.size( ) ) {
							next_ranges.push_back( ranges.back( ) );
						}
						ranges = next_ranges;
					}
				}

				template<size_t MinRangeSize = 512, typename Iterator, typename Sort, typename Compare>
				void parallel_sort( Iterator first, Iterator last, Sort sort, Compare compare ) {
					static_assert( MinRangeSize > 0, "MinRangeSize cannot be 0" );
					if( MinRangeSize > static_cast<size_t>( std::distance( first, last ) ) ) {
						sort( first, last, compare );
						return;
					}
					daw::locked_stack_t<std::pair<Iterator, Iterator>> sort_ranges_stack;
					partition_range<MinRangeSize>( first, last,
					                               [&sort_ranges_stack, sort, compare]( Iterator f, Iterator l ) {
						                               sort( f, l, compare );
						                               sort_ranges_stack.push_back( std::make_pair( f, l ) );
					                               } );

					size_t const expected_results =
					    get_part_info<MinRangeSize>( first, last, get_task_scheduler( ).size( ) ).count;
					std::vector<std::pair<Iterator, Iterator>> sort_ranges;
					for( size_t n = 0; n < expected_results; ++n ) {
						sort_ranges.push_back( sort_ranges_stack.pop_back2( ) );
					}
					std::sort( sort_ranges.begin( ), sort_ranges.end( ),
					           []( auto const &lhs, auto const &rhs ) { return lhs.first < rhs.first; } );

					merge_reduce_range( sort_ranges, [compare]( Iterator f, Iterator m, Iterator l ) {
						std::inplace_merge( f, m, l, compare );
					} );
				}

				template<typename T, typename Iterator, typename BinaryOp>
				T parallel_reduce( Iterator first, Iterator last, T init, BinaryOp binary_op ) {
					{
						auto const sz = static_cast<size_t>( std::distance( first, last ) );
						if( sz < 2 ) {
							if( sz == 0 ) {
								return std::move( init );
							}
							return binary_op( init, *first );
						}
					}
					daw::locked_stack_t<T> results;
					auto t1 =
					    impl::partition_range<1>( first, last, [&results, binary_op]( Iterator f, Iterator const l ) {
						    auto result = binary_op( *f, *std::next( f ) );
						    std::advance( f, 2 );
						    for( ; f != l; std::advance( f, 1 ) ) {
							    result = binary_op( result, *f );
						    }
						    results.push_back( std::move( result ) );
					    } );
					auto result = std::move( init );
					size_t const expected_results =
					    get_part_info<1>( first, last, get_task_scheduler( ).size( ) ).count;
					for( size_t n = 0; n < expected_results; ++n ) {
						result = binary_op( result, results.pop_back2( ) );
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

				template<size_t MinRangeSize = 1, typename Iterator, typename Compare>
				auto parallel_min_element( Iterator first, Iterator last, Compare cmp ) {
					if( first == last ) {
						return last;
					}
					daw::locked_stack_t<Iterator> results;
					auto t1 = impl::partition_range<MinRangeSize>(
					    first, last, [&results, cmp]( Iterator const f, Iterator const l ) {
						    auto const sz = static_cast<size_t>( std::distance( f, l ) );
						    auto result_val = *f;
						    auto result_pos = 0;
						    for( size_t n = 1; n < sz; ++n ) {
							    auto v = f[n];
							    if( cmp( v, result_val ) ) {
								    result_pos = n;
								    result_val = std::move( v );
							    }
						    }
						    results.push_back( std::next( f, result_pos ) );
					    } );

					size_t const expected_results =
					    get_part_info<MinRangeSize>( first, last, get_task_scheduler( ).size( ) ).count;

					auto result = results.pop_back2( );
					size_t start_pos = 1;
					while( start_pos < expected_results && result == last ) {
						++start_pos;
						result = results.pop_back2( );
					}
					for( size_t n = start_pos; n < expected_results; ++n ) {
						auto value = results.pop_back2( );
						if( value == last ) {
							continue;
						}
						if( result != last ) {
							result = cmp( *value, *result ) ? value : result;
						} else {
							result = value;
						}
					}
					return result;
				}

				template<size_t MinRangeSize = 1, typename Iterator, typename Compare>
				auto parallel_max_element( Iterator first, Iterator last, Compare cmp ) {
					if( first == last ) {
						return last;
					}
					daw::locked_stack_t<Iterator> results;
					auto t1 = impl::partition_range<MinRangeSize>(
					    first, last, [&results, cmp]( Iterator const f, Iterator const l ) {
						    auto const sz = static_cast<size_t>( std::distance( f, l ) );
						    auto result_val = *f;
						    size_t result_pos = 0;
						    for( size_t n = 1; n < sz; ++n ) {
							    auto v = f[n];
							    if( cmp( result_val, v ) ) {
								    result_pos = n;
								    result_val = std::move( v );
							    }
						    }
						    results.push_back( std::next( f, result_pos ) );
					    } );

					size_t const expected_results =
					    get_part_info<MinRangeSize>( first, last, get_task_scheduler( ).size( ) ).count;

					auto result = results.pop_back2( );
					size_t start_pos = 1;
					while( start_pos < expected_results && result == last ) {
						++start_pos;
						result = results.pop_back2( );
					}
					for( size_t n = start_pos; n < expected_results; ++n ) {
						auto value = results.pop_back2( );
						if( value == last ) {
							continue;
						}
						if( result != last ) {
							result = cmp( *result, *value ) ? value : result;
						} else {
							result = value;
						}
					}
					return result;
				}

				template<size_t MinRangeSize = 512, typename Iterator, typename OutputIterator, typename UnaryOperation>
				void parallel_map( Iterator first1, Iterator const last1, OutputIterator first2,
				                   UnaryOperation unary_op ) {

					partition_range<MinRangeSize>( first1, last1,
					                               [first1, first2, unary_op]( Iterator f, Iterator const l ) {
						                               auto out_it = std::next( first2, std::distance( first1, f ) );

						                               for( ; f != l; ++f, ++out_it ) {
							                               *out_it = unary_op( *f );
						                               }
					                               } )
					    .wait( );
				}

				template<size_t MinRangeSize = 512, typename Iterator, typename T, typename MapFunction,
				         typename ReduceFunction>
				auto parallel_map_reduce( Iterator first, Iterator last, T const &init, MapFunction map_function,
				                          ReduceFunction reduce_function ) {
					static_assert( MinRangeSize >= 2, "MinRangeSize must be >= 2" );
					daw::exception::daw_throw_on_false( std::distance( first, last ) >= 2,
					                                    "Must be at least 2 items in range" );
					using result_t = std::decay_t<decltype( reduce_function(
					    map_function( *std::declval<Iterator>( ) ), map_function( *std::declval<Iterator>( ) ) ) )>;
					daw::locked_stack_t<result_t> results;

					partition_range<MinRangeSize>(
					    first, last, [&results, map_function, reduce_function]( Iterator f, Iterator const l ) {
						    result_t result = reduce_function( map_function( *f ), map_function( *std::next( f ) ) );
						    std::advance( f, 2 );
						    for( ; f != l; ++f ) {
							    result = reduce_function( result, map_function( *f ) );
						    }
						    results.push_back( std::move( result ) );
					    } );

					auto result = map_function( init );
					size_t const expected_results =
					    get_part_info<MinRangeSize>( first, last, get_task_scheduler( ).size( ) ).count;
					for( size_t n = 0; n < expected_results; ++n ) {
						result = reduce_function( result, results.pop_back2( ) );
					}
					return result;
				}

				template<size_t MinRangeSize = 512, typename Iterator, typename OutputIterator, typename BinaryOp>
				void parallel_scan( Iterator first, Iterator last, OutputIterator first_out, OutputIterator last_out,
				                    BinaryOp binary_op ) {
					{
						auto const sz = static_cast<size_t>( std::distance( first, last ) );
						daw::exception::daw_throw_on_false( sz != 0, "Range cannot be empty" );
						daw::exception::daw_throw_on_false(
						    sz == static_cast<size_t>( std::distance( first_out, last_out ) ),
						    "Output range must be the same size as input" );
						if( sz < 2 ) {
							*first_out = *first;
							return;
						}
					}
					using value_t = std::decay_t<decltype( binary_op( *first, *first ) )>;
					struct result_t {
						iterator_range_t<Iterator> range;
						value_t value;
					};

					auto ts = get_task_scheduler( );
					auto ranges = split_range_t<Iterator, MinRangeSize>{}( first, last, ts.size( ) );
					daw::locked_stack_t<result_t> locked_results;
					// Do partial sum on each sub range and return last value
					parallel_for_each( ranges.cbegin( ), ranges.cend( ),
					                   [&locked_results, binary_op]( iterator_range_t<Iterator> rng ) {
						                   result_t result{rng, rng.pop_front( )};
						                   while( !rng.empty( ) ) {
							                   result.value += rng.pop_front( );
						                   }
						                   locked_results.push_back( std::move( result ) );
					                   } );

					auto results = locked_results.copy( );
					std::sort( results.begin( ), results.end( ), []( auto const &lhs, auto const &rhs ) {
						return lhs.range.cbegin( ) < rhs.range.cbegin( );
					} );
					{
						std::vector<value_t> values;
						values.resize( results.size( ) );
						for( size_t n = 1; n < results.size( ); ++n ) {
							values[n] = results[0].value;
							for( size_t m = 1; m < n; ++m ) {
								values[n] += results[m].value;
							}
						}
						for( size_t n = 0; n < results.size( ); ++n ) {
							results[n].value = values[n];
						}
					}

					parallel_for_each(
					    results.begin( ), results.end( ), [first, first_out, binary_op]( result_t cur_range ) {
						    auto out_pos = std::next( first_out, std::distance( first, cur_range.range.first ) );
						    auto sum = binary_op( cur_range.value, cur_range.range.pop_front( ) );
						    *( out_pos++ ) = sum;
						    while( !cur_range.range.empty( ) ) {
							    sum = binary_op( sum, cur_range.range.pop_front( ) );
							    *( out_pos++ ) = sum;
						    }
					    } );
				}
			} // namespace impl
		}     // namespace parallel
	}         // namespace algorithm
} // namespace daw
