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
					auto const sz = std::distance( first, last );
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
					if( result.count == 0 ) {
						result.count = 1;
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
				template<size_t MinRangeSize = 512, typename Iterator, typename Compare>
				void parallel_sort( Iterator first, Iterator last, Compare compare ) {
					if( MinRangeSize >= static_cast<size_t>( std::distance( first, last ) ) ) {
						std::sort( first, last, compare );
						return;
					}
					partition_range<MinRangeSize>( first, last,
					                               [compare]( Iterator f, Iterator l ) { std::sort( f, l, compare ); } )
					    ->wait( );
					auto ts = get_task_scheduler( );
					auto part_info = get_part_info<MinRangeSize>( first, last, ts.size( ) );
					while( part_info.count > 1 ) {
						auto count = part_info.count;
						if( count % 2 == 1 ) {
							--count;
						}
						auto semaphore = std::make_shared<daw::semaphore>( 1 - ( count / 2 ) );
						for( size_t n = 0; n < count; n += 2 ) {
							auto f = daw::algorithm::safe_next( first, last, n * part_info.size );
							auto m = daw::algorithm::safe_next( first, last, ( n + 1 ) * part_info.size );
							auto l = daw::algorithm::safe_next( first, last, ( n + 2 ) * part_info.size );
							ts.add_task( [f, m, l, compare, semaphore]( ) {
								std::inplace_merge( f, m, l, compare );
								semaphore->notify( );
							} );
						}
						semaphore->wait( );
						if( part_info.count % 2 == 1 ) {
							++part_info.count;
						}
						part_info.count /= 2;
						part_info.size *= 2;
					}
				}
			} // namespace impl

			template<typename Iterator,
			         typename Compare = std::less<typename std::iterator_traits<Iterator>::value_type>>
			void sort( Iterator first, Iterator last, Compare compare = Compare{} ) {
				impl::parallel_sort( first, last, std::move( compare ) );
			}
		} // namespace parallel
	}     // namespace algorithm
} // namespace daw
