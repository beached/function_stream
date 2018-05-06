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

#pragma once

#include <algorithm>

#include <daw/cpp_17.h>

#include "impl/concept_checks.h"
#include "task_scheduler.h"

namespace daw {
	namespace algorithm {
		namespace parallel {
			namespace impl {
				enum class sort_dir_t : bool { up = true, down = false };
				template<sort_dir_t>
				struct not_dir_t;
				template<>
				struct not_dir_t<sort_dir_t::up> {
					static constexpr sort_dir_t const value = sort_dir_t::down;
				};
				template<>
				struct not_dir_t<sort_dir_t::down> {
					static constexpr sort_dir_t const value = sort_dir_t::up;
				};

				constexpr size_t
				greatest_power_of_two_less_than( size_t const n ) noexcept {
					size_t k = 1u;
					while( k > 0u && k < n ) {
						k = k << 1u;
					}
					return k >> 1u;
				}

				template<sort_dir_t sort_dir, typename Compare>
				struct compare_exchange_t;

				template<typename Compare>
				struct compare_exchange_t<sort_dir_t::up, Compare> {
					template<typename T>
					constexpr void operator( )( T &lhs, T &rhs ) noexcept {
						if( Compare{}( rhs, lhs ) ) {
							using std::swap;
							swap( lhs, rhs );
						}
					}
				};

				template<typename Compare>
				struct compare_exchange_t<sort_dir_t::down, Compare> {
					template<typename T>
					constexpr void operator( )( T &lhs, T &rhs ) noexcept {
						if( !Compare{}( rhs, lhs ) ) {
							using std::swap;
							swap( lhs, rhs );
						}
					}
				};
				/*
				template<sort_dir_t sort_dir, typename Compare, typename T>
				constexpr void compare_exchange( T &lhs, T &rhs ) noexcept {
				  if( static_cast<bool>( sort_dir ) == Compare{}( rhs, lhs ) ) {
				    using std::swap;
				    swap( lhs, rhs );
				  }
				}
				*/
				template<sort_dir_t sort_dir, typename Compare, typename T>
				constexpr void bitonic_merge( T *const first,
				                              size_t const N ) noexcept {
					if( N <= 1 ) {
						return;
					}

					auto const mid = greatest_power_of_two_less_than( N );
					for( size_t n = 0u; n < mid; ++n ) {
						compare_exchange_t<sort_dir, Compare>{}( first[n], first[n + mid] );
					}

					bitonic_merge<sort_dir, Compare>( first, mid );
					bitonic_merge<sort_dir, Compare>(
					  std::next( first, static_cast<intmax_t>( mid ) ), N - mid );
				}

				template<sort_dir_t sort_dir, typename Compare, typename T>
				constexpr void bitonic_sort( T *const first, size_t const N ) noexcept {
					if( N <= 1 ) {
						return;
					}
					auto const mid = N / 2;
					bitonic_sort<not_dir_t<sort_dir>::value, Compare>( first, mid );
					bitonic_sort<sort_dir, Compare>(
					  std::next( first, static_cast<intmax_t>( mid ) ), N - mid );

					bitonic_merge<sort_dir, Compare>( first, N );
				}

				template<sort_dir_t sort_dir, typename Compare,
				         typename PartitionPolicy, typename T>
				void par_bitonic_merge( T *first, size_t N, daw::task_scheduler ts ) {
					if( N <= static_cast<intmax_t>( PartitionPolicy::min_range_size ) ) {
						bitonic_merge<sort_dir, Compare>( first, N );
						return;
					}

					auto const mid = greatest_power_of_two_less_than( N );
					for( size_t n = 0u; n < mid; ++n ) {
						compare_exchange_t<sort_dir, Compare>{}( first[n], first[n + mid] );
					}

					daw::invoke_tasks(
					  [first, mid, ts]( ) {
						  par_bitonic_merge<sort_dir, Compare, PartitionPolicy>( first, mid,
						                                                         ts );
					  },
					  [first, mid, N, ts]( ) {
						  par_bitonic_merge<sort_dir, Compare, PartitionPolicy>(
						    std::next( first, static_cast<intmax_t>( mid ) ), N - mid, ts );
					  } );
				}

				template<sort_dir_t sort_dir, typename Compare,
				         typename PartitionPolicy, typename T>
				void par_bitonic_sort( T *first, size_t N, daw::task_scheduler ts ) {
					if( N <= static_cast<intmax_t>( PartitionPolicy::min_range_size ) ) {
						bitonic_sort<sort_dir, Compare>( first, N );
						return;
					}

					auto const mid = N / 2;
					daw::invoke_tasks(
					  [first, mid, ts]( ) {
						  par_bitonic_sort<not_dir_t<sort_dir>::value, Compare,
						                   PartitionPolicy>( first, mid, ts );
					  },
					  [first, mid, N, ts]( ) {
						  par_bitonic_sort<sort_dir, Compare, PartitionPolicy>(
						    std::next( first, static_cast<intmax_t>( mid ) ), N - mid, ts );
					  } );

					par_bitonic_merge<sort_dir, Compare, PartitionPolicy>( first, N, ts );
				}
			} // namespace impl
			template<typename RandomIterator,
			         typename Compare = std::less<
			           typename std::iterator_traits<RandomIterator>::value_type>>
			constexpr void seq_bitonic_sort( RandomIterator first,
			                                 RandomIterator const last ) {

				static_assert(
				  daw::concept_checks::is_binary_predicate_v<Compare, RandomIterator,
				                                             RandomIterator>,
				  "Supplied Compare does not satisfy the concept of BinaryPredicate.  "
				  "See "
				  "http://en.cppreference.com/w/cpp/concept/BinaryPredicate" );

				impl::bitonic_sort<impl::sort_dir_t::up, Compare>(
				  &( *first ), static_cast<size_t>( std::distance( first, last ) ) );
			}
			template<typename RandomIterator,
			         typename Compare = std::less<
			           typename std::iterator_traits<RandomIterator>::value_type>>
			void bitonic_sort( RandomIterator first, RandomIterator const last,
			                   daw::task_scheduler ts = daw::get_task_scheduler( ) ) {
				static_assert(
				  daw::concept_checks::is_binary_predicate_v<Compare, RandomIterator,
				                                             RandomIterator>,
				  "Supplied Compare does not satisfy the concept of BinaryPredicate.  "
				  "See "
				  "http://en.cppreference.com/w/cpp/concept/BinaryPredicate" );

				impl::par_bitonic_sort<impl::sort_dir_t::up, Compare,
				                       impl::split_range_t<65535u>>(
				  &( *first ), static_cast<size_t>( std::distance( first, last ) ),
				  std::move( ts ) );
			}
		} // namespace parallel
	}   // namespace algorithm
} // namespace daw
