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

				template<size_t MinRangeSize = 1, typename Iterator, typename Func>
				auto partition_range( Iterator first, Iterator const last, Func func ) {
					auto const sz = std::distance( first, last );
					if( sz == 0 ) {
						return std::make_shared<daw::semaphore>( 1 );
					}
					auto ts = get_task_scheduler( );
					auto const part_info = get_part_info<MinRangeSize>( first, last, ts.size( ) );
					auto semaphore = std::make_shared<daw::semaphore>( 1 - part_info.count );
					auto last_pos = first;
					first = daw::algorithm::safe_next( first, last, part_info.size );
					for( ; first != last; first = daw::algorithm::safe_next( first, last, part_info.size ) ) {
						ts.add_task( [last_pos, first, func, semaphore]( ) {
							func( last_pos, first );
							semaphore->notify( );
						} );
						last_pos = first;
					}
					ts.add_task( [last_pos, first, func, semaphore]( ) {
						func( last_pos, first );
						semaphore->notify( );
					} );
					return semaphore;
				}
			} // namespace impl

			template<typename RandomIterator, typename Func>
			void for_each( RandomIterator const first, RandomIterator const last, Func func ) {
				impl::partition_range( first, last,
				                       [func]( RandomIterator f, RandomIterator l ) {
					                       for( auto it = f; it != l; ++it ) {
						                       func( *it );
					                       }
				                       } )
				    ->wait( );
			}

			template<typename Iterator, typename Func>
			void for_each_n( Iterator first, size_t N, Func func ) {
				daw::algorithm::parallel::for_each( first, first + N, func );
			}

			template<typename Iterator, typename T>
			void fill( Iterator first, Iterator last, T const &value ) {
				daw::algorithm::parallel::for_each( first, last, [&value]( auto &item ) { item = value; } );
			}

			namespace impl {
				template<typename Iterator, typename Function>
				void merge_reduce_range( std::vector<std::pair<Iterator, Iterator>> ranges, Function func ) {
					auto ts = get_task_scheduler( );
					while( ranges.size( ) > 1 ) {
						std::vector<std::pair<Iterator, Iterator>> next_ranges;
						auto const count = ( ranges.size( ) % 2 == 0 ? ranges.size( ) : ranges.size( ) - 1 );
						auto semaphore = std::make_shared<daw::semaphore>( 1 - ( count / 2 ) );
						for( size_t n = 1; n < count; n += 2 ) {
							daw::exception::daw_throw_on_false( ranges[n - 1].second == ranges[n].first,
							                                    "Non continuous range" );

							ts.add_task( [func, &ranges, n, semaphore]( ) {
								func( ranges[n - 1].first, ranges[n].first, ranges[n].second );
								semaphore->notify( );
							} );
							next_ranges.push_back( std::make_pair( ranges[n - 1].first, ranges[n].second ) );
						}
						semaphore->wait( );
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
					impl::partition_range( first, last,
					                       [&sort_ranges_stack, sort, compare]( Iterator f, Iterator l ) {
						                       sort_ranges_stack.push_back( std::make_pair( f, l ) );
						                       sort( f, l, compare );
					                       } )
					    ->wait( );

					std::vector<std::pair<Iterator, Iterator>> sort_ranges;
					while( sort_ranges_stack.size( ) > 0 ) {
						sort_ranges.push_back( sort_ranges_stack.pop_back( ) );
					}
					std::sort( sort_ranges.begin( ), sort_ranges.end( ),
					           []( auto const &lhs, auto const &rhs ) { return lhs.first < rhs.first; } );
					merge_reduce_range( sort_ranges, [compare]( Iterator f, Iterator m, Iterator l ) {
						std::inplace_merge( f, m, l, compare );
					} );
				}

				template<typename Iterator, typename T, typename BinaryOp>
				T parallel_accumulate( Iterator first, Iterator last, T init, BinaryOp binary_op ) {
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
					    impl::partition_range( first, last, [&results, binary_op]( Iterator f, Iterator const l ) {
						    auto result = binary_op( *f, *std::next( f ) );
						    std::advance( f, 2 );
						    for( ; f != l; std::advance( f, 1 ) ) {
							    result = binary_op( result, *f );
						    }
						    results.push_back( std::move( result ) );
					    } );
					// TODO: determine if it is worth parallizing below.
					// Not sure as it should be N=num threads
					auto result = std::move( init );
					auto const expected_results = get_part_info<1>( first, last, get_task_scheduler( ).size( ) ).count;
					for( size_t n=0; n<expected_results; ++n ) {
						result = binary_op( result, results.pop_back2( ) );
					}
					return result;
				}
			} // namespace impl

			template<typename Iterator,
			         typename Compare = std::less<typename std::iterator_traits<Iterator>::value_type>>
			void sort( Iterator first, Iterator last, Compare compare = Compare{} ) {
				impl::parallel_sort( first, last, []( Iterator f, Iterator l, Compare cmp ) { std::sort( f, l, cmp ); },
				                     std::move( compare ) );
			}

			template<typename Iterator,
			         typename Compare = std::less<typename std::iterator_traits<Iterator>::value_type>>
			void stable_sort( Iterator first, Iterator last, Compare compare = Compare{} ) {
				impl::parallel_sort( first, last,
				                     []( Iterator f, Iterator l, Compare cmp ) { std::stable_sort( f, l, cmp ); },
				                     std::move( compare ) );
			}

			template<typename Iterator, typename T, typename BinaryOp>
			T accumulate( Iterator first, Iterator last, T && init, BinaryOp binary_op ) {
				return impl::parallel_accumulate( first, last, std::forward<T>( init ), binary_op );
			}

			template<typename Iterator, typename T>
			auto accumulate( Iterator first, Iterator last, T && init ) {
				using value_type = typename std::iterator_traits<Iterator>::value_type;
				return ::daw::algorithm::parallel::accumulate(
				    first, last, std::forward<T>( init ),
				    []( auto const &lhs, auto const &rhs ) -> value_type { return lhs + rhs; } );
			}

			template<typename Iterator>
			auto accumulate( Iterator first, Iterator last ) {
				using value_type = typename std::iterator_traits<Iterator>::value_type;
				return ::daw::algorithm::parallel::accumulate( first, last, value_type{} );
			}

		} // namespace parallel
	}     // namespace algorithm
} // namespace daw
