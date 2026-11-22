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

#pragma once

#include "../future_result.h"
#include "../task_scheduler.h"
#include "daw_latch.h"

#include <daw/daw_algorithm.h>
#include <daw/daw_mutable_capture.h>
#include <daw/daw_scope_guard.h>
#include <daw/daw_view.h>
#include <daw/parallel/daw_spin_lock.h>

#include <algorithm>
#include <atomic>
#include <exception>
#include <iterator>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>

namespace daw::algorithm::parallel::impl {
	template<size_t MinRangeSize = 1>
	struct [[nodiscard]] split_range_t {
		static_assert( MinRangeSize != 0, "Minimum range size must be > 0" );
		static constexpr size_t min_range_size = MinRangeSize;

		template<typename Iterator>
		[[nodiscard]] std::vector<daw::view<Iterator>>
		operator( )( Iterator first, Iterator last, size_t const max_parts ) const {

			return daw::algorithm::partition_range<MinRangeSize>(
			  first, last, max_parts );
		}

		template<typename Iterator>
		[[nodiscard]] std::vector<daw::view<Iterator>>
		operator( )( daw::view<Iterator> rng, size_t const max_parts ) const {
			return operator( )( rng.begin( ), rng.end( ), max_parts );
		}
	};

	template<typename RandomIterator, typename Func>
	[[nodiscard]] daw::shared_cnt_sem
	partition_range( std::vector<daw::view<RandomIterator>> ranges, Func &&func,
	                 task_scheduler ts ) {
		// Each scheduled task adds its own notifier; the initial count of 1 keeps the
		// semaphore from reaching 0 before every task has been scheduled
		auto sem = daw::shared_cnt_sem( 1 );
		auto const ae = on_scope_exit( [sem]( ) mutable {
			sem.notify( );
		} );
		for( auto rng : ranges ) {
			if( not schedule_task(
			      sem,
			      [fn = daw::mutable_capture( func ), range = std::move( rng )] {
				      ( *fn )( range );
			      },
			      ts ) ) {

				throw daw::unable_to_add_task_exception{ };
			}
		}
		return sem;
	}

	template<typename RandomIterator, typename Func>
	[[nodiscard]] daw::shared_cnt_sem
	partition_range_pos( std::vector<daw::view<RandomIterator>> ranges, Func func,
	                     task_scheduler ts,
	                     size_t const start_pos = 0 ) noexcept {
		auto sem = daw::shared_cnt_sem( 1 );
		auto const ae = on_scope_exit( [sem]( ) mutable {
			sem.notify( );
		} );
		for( size_t n = start_pos; n < ranges.size( ); ++n ) {
			if( not schedule_task(
			      sem,
			      [fn = daw::mutable_capture( func ), rng = ranges[n], n]( ) {
				      fn.move_out( )( rng, n );
			      },
			      ts ) ) {

				// Unable to add task, consider a better error mechanism like optional
				// with the latch reduced to match the number of items
				std::terminate( );
			}
		}
		return sem;
	}

	template<typename PartitionPolicy, random_access_iterator RandomIterator,
	         typename Func>
	[[nodiscard]] shared_cnt_sem partition_range( daw::view<RandomIterator> range,
	                                              Func &&func,
	                                              task_scheduler ts ) noexcept {
		if( range.empty( ) ) {
			return shared_cnt_sem( 0 );
		}
		auto const ranges = PartitionPolicy{ }( range, ts.size( ) );
		auto sem = daw::shared_cnt_sem( 1 );
		auto const ae = on_scope_exit( [sem]( ) mutable {
			sem.notify( );
		} );
		for( auto rng : ranges ) {
			if( not schedule_task(
			      sem,
			      [fn = daw::mutable_capture( DAW_FWD( func ) ), rng]( ) {
				      fn.move_out( )( rng.begin( ), rng.end( ) );
			      },
			      ts ) ) {

				// TODO: handle this
				std::terminate( );
			}
		}
		return sem;
	}

	template<typename PartitionPolicy = split_range_t<>, typename RandomIterator,
	         typename Func>
	void parallel_for_each( daw::view<RandomIterator> rng, Func &&func,
	                        task_scheduler ts ) {

		static_assert( std::is_invocable_v<Func, decltype( rng.front( ) )> );

		ts.wait_for( partition_range<PartitionPolicy>(
		  rng,
		  [fn = daw::mutable_capture( DAW_FWD( func ) )]( auto &&first,
			                                                auto &&last ) {
			  while( first != last ) {
				  ( *fn )( *first );
				  ++first;
			  }
		  },
		  ts ) );
	}

	template<typename Ranges, typename Func>
	void parallel_for_each( Ranges &ranges, Func func, task_scheduler ts ) {
		ts.wait_for( partition_range(
		  ranges,
		  [func]( auto f, auto l ) mutable {
			  for( auto it = f; it != l; ++it ) {
				  func( *it );
			  }
		  },
		  ts ) );
	}

	template<typename PartitionPolicy = split_range_t<>, typename RandomIterator,
	         typename Func>
	void parallel_for_each_index( RandomIterator first, RandomIterator last,
	                              Func func, task_scheduler ts ) {
		auto const ranges = PartitionPolicy{ }( first, last, ts.size( ) );
		auto sem = daw::shared_cnt_sem( ranges.size( ) );
		Unused( sem );
		partition_range(
		  ranges,
		  [func, first]( auto rng ) {
			  auto const start_pos =
			    static_cast<size_t>( std::distance( first, rng.begin( ) ) );
			  auto const end_pos =
			    static_cast<size_t>( std::distance( first, rng.end( ) ) );
			  for( size_t n = start_pos; n < end_pos; ++n ) {
				  func( n );
			  }
		  },
		  ts )
		  .wait( );
	}

	/// Records the first exception thrown while sorting or merging.  parallel_sort
	/// catches exceptions instead of letting them travel through the futures, because a
	/// failed future completes the next one straight away, while other sorts and merges
	/// are still using the caller's range and the merge buffer
	struct sort_error_t {
		std::atomic<bool> failed = false;
		std::mutex mut;
		std::exception_ptr ptr;

		void set( std::exception_ptr p ) {
			auto const lck = std::lock_guard( mut );
			if( not ptr ) {
				ptr = std::move( p );
			}
			failed.store( true, std::memory_order_relaxed );
		}

		[[nodiscard]] bool has_failed( ) const {
			return failed.load( std::memory_order_relaxed );
		}
	};

	/// Uninitialized scratch space for parallel_sort's merges, one slot per element of
	/// the range being sorted.  Allocated once per sort instead of std::inplace_merge
	/// allocating a temporary buffer for every merge
	template<typename T>
	class merge_buffer_t {
		T *m_ptr = nullptr;
		std::size_t m_size = 0;

	public:
		explicit merge_buffer_t( std::size_t size )
		  : m_ptr( size > 0 ? std::allocator<T>{ }.allocate( size ) : nullptr )
		  , m_size( size ) {}

		merge_buffer_t( merge_buffer_t const & ) = delete;
		merge_buffer_t &operator=( merge_buffer_t const & ) = delete;

		~merge_buffer_t( ) {
			if( m_ptr ) {
				std::allocator<T>{ }.deallocate( m_ptr, m_size );
			}
		}

		[[nodiscard]] T *data( ) const noexcept {
			return m_ptr;
		}
	};

	/// Merges two adjacent sorted ranges through a shared scratch buffer.  A merge of
	/// [left.begin, right.end) only uses the buffer slots at the same offsets from base,
	/// so merges of disjoint ranges can run concurrently.  The merge is stable, so this
	/// also serves stable_sort
	template<typename Compare, typename Iterator>
	struct parallel_sort_merger {
		using value_type = std::iter_value_t<Iterator>;

		Compare cmp;
		Iterator base;
		value_type *buffer;
		sort_error_t *error;

		daw::view<Iterator> operator( )( daw::view<Iterator> rng_left,
		                                 daw::view<Iterator> rng_right ) const {
			daw::exception::dbg_precondition_check(
			  rng_left.end( ) == rng_right.begin( ), "Ranges must be contigous" );

			auto const result = daw::view<Iterator>( rng_left.begin( ), rng_right.end( ) );
			if( error->has_failed( ) ) {
				// The sort will throw anyway, skip the work
				return result;
			}
			try {
				merge( rng_left, rng_right );
			} catch( ... ) { error->set( std::current_exception( ) ); }
			return result;
		}

	private:
		void merge( daw::view<Iterator> rng_left, daw::view<Iterator> rng_right ) const {
			if( rng_left.empty( ) or rng_right.empty( ) or
			    not cmp( *rng_right.begin( ), *std::prev( rng_left.end( ) ) ) ) {
				// Already in order
				return;
			}
			// Move the left half out to its slots in the buffer, then merge it with the
			// right half (still in place) back into the range.  The write position never
			// passes the read position in the right half, so nothing is overwritten early
			value_type *const buf_first =
			  buffer + std::distance( base, rng_left.begin( ) );
			value_type *const buf_last =
			  std::uninitialized_move( rng_left.begin( ), rng_left.end( ), buf_first );
			auto const destroy_buffer = daw::on_scope_exit( [&]( ) noexcept {
				std::destroy( buf_first, buf_last );
			} );

			value_type *lhs = buf_first;
			auto rhs = rng_right.begin( );
			auto const rhs_last = rng_right.end( );
			auto out = rng_left.begin( );
			try {
				while( lhs != buf_last and rhs != rhs_last ) {
					// Take from the right only when strictly less, keeping it stable
					if( cmp( *rhs, *lhs ) ) {
						*out = std::move( *rhs );
						++rhs;
					} else {
						*out = std::move( *lhs );
						++lhs;
					}
					++out;
				}
			} catch( ... ) {
				// Put the remaining buffered elements back so none are lost
				std::move( lhs, buf_last, out );
				throw;
			}
			// Whatever remains of the right half is already in its final place
			std::move( lhs, buf_last, out );
		}
	};

	struct sorter_t {
		void operator( )(
		  auto first, auto last,
		  invocable_result<bool, iter_reference_t<DAW_TYPEOF( first )>,
			                 iter_reference_t<DAW_TYPEOF( first )>> auto cmp ) const {
			std::sort( std::move( first ), std::move( last ), std::move( cmp ) );
		}
	};
	inline constexpr auto sorter = sorter_t{ };

	struct stable_sorter_t {
		void operator( )(
		  auto first, auto last,
		  invocable_result<bool, iter_reference_t<DAW_TYPEOF( first )>,
			                 iter_reference_t<DAW_TYPEOF( first )>> auto cmp ) const {
			std::stable_sort( std::move( first ), std::move( last ), DAW_FWD( cmp ) );
		}
	};
	inline constexpr auto stable_sorter = stable_sorter_t{ };

	template<typename PartitionPolicy = split_range_t<>, typename Iterator,
	         typename Sort, typename Compare>
	void parallel_sort( daw::view<Iterator> range, Sort &&srt, Compare &&cmp,
	                    task_scheduler ts ) {
		if( PartitionPolicy::min_range_size > range.size( ) ) {
			srt( range.begin( ), range.end( ), cmp );
			return;
		}
		auto ranges = PartitionPolicy( )( range, ts.size( ) );
		// Scratch space for the merges, only needed when there is something to merge.
		// Allocated before any task is scheduled, so a bad_alloc leaves nothing running
		auto const buffer = merge_buffer_t<std::iter_value_t<Iterator>>(
		  ranges.size( ) > 1 ? range.size( ) : 0 );

		auto sorters = std::vector<future_result_t<daw::view<Iterator>>>( );
		sorters.reserve( ranges.size( ) );

		// Must outlive every task, which holds because we wait for all of them below
		auto error = sort_error_t( );

		auto const sort_fn =
		  [comp = mutable_capture( cmp ),
			 sort = mutable_capture( DAW_FWD( srt ) ),
			 err = &error]( daw::view<Iterator> r ) {
			  if( not err->has_failed( ) ) {
				  try {
					  ( *sort )( r.begin( ), r.end( ), *comp );
				  } catch( ... ) { err->set( std::current_exception( ) ); }
			  }
			  return r;
		  };

		daw::algorithm::transform( ranges.begin( ),
		                           ranges.end( ),
		                           std::back_inserter( sorters ),
		                           [tsk_sch = daw::mutable_capture( ts ),
		                            sort_fn]( daw::view<Iterator> rng ) {
			                           return make_future_result(
			                             *tsk_sch, sort_fn, rng );
		                           } );

		using merger_t = parallel_sort_merger<daw::remove_cvref_t<Compare>, Iterator>;
		ts.wait_for( reduce_futures(
		  sorters.begin( ),
		  sorters.end( ),
		  merger_t{ cmp, range.begin( ), buffer.data( ), &error } ) );
		// Every sort and merge has finished, so it is now safe to report a failure
		if( error.ptr ) {
			std::rethrow_exception( error.ptr );
		}
	}

	template<typename PartitionPolicy = split_range_t<>, typename T,
	         typename Iterator, typename BinaryOp>
	[[nodiscard]] auto parallel_reduce( daw::view<Iterator> range, T init,
	                                    BinaryOp binary_op, task_scheduler ts ) {
		using result_t =
		  daw::remove_cvref_t<decltype( binary_op( init, range.front( ) ) )>;
		{
			if( range.size( ) < 2 ) {
				if( range.empty( ) ) {
					return static_cast<result_t>( init );
				}
				return binary_op( init, range.front( ) );
			}
		}
		auto const ranges = PartitionPolicy{ }( range, ts.size( ) );
		auto results = std::vector<std::optional<T>>( ranges.size( ) );
		auto sem = partition_range_pos(
		  ranges,
		  [&results, binary_op]( daw::view<Iterator> rng, size_t n ) {
			  results[n] = std::accumulate(
			    std::next( rng.cbegin( ) ), rng.cend( ), rng.front( ), binary_op );
		  },
		  ts );
		ts.wait_for( sem );
		// At this point we know that all results optional have values
		auto result = static_cast<result_t>( init );
		for( size_t n = 0; n < ranges.size( ); ++n ) {
			result = binary_op( result, *results[n] );
		}
		return result;
	}

	template<typename PartitionPolicy = split_range_t<>, typename Iterator,
	         typename Compare>
	[[nodiscard]] Iterator parallel_min_element( daw::view<Iterator> range,
	                                             Compare cmp,
	                                             task_scheduler ts ) {
		if( range.empty( ) ) {
			return range.end( );
		}
		struct min_element_worker {
			std::vector<Iterator> &r;
			Compare c;

			inline void operator( )( daw::view<Iterator> rng, size_t n ) const {
				r[n] = std::min_element( rng.cbegin( ), rng.cend( ), c );
			}
		};
		auto const ranges = PartitionPolicy{ }( range, ts.size( ) );
		auto results = std::vector<Iterator>( ranges.size( ), range.end( ) );
		auto sem =
		  partition_range_pos( ranges, min_element_worker{ results, cmp }, ts );
		ts.wait_for( sem );

		return *std::min_element( results.cbegin( ),
		                          results.cend( ),
		                          [cmp]( auto const &lhs, auto const &rhs ) {
			                          return cmp( *lhs, *rhs );
		                          } );
	}

	template<typename PartitionPolicy = split_range_t<>, typename Iterator,
	         typename Compare>
	[[nodiscard]] Iterator parallel_max_element( daw::view<Iterator> range,
	                                             Compare cmp,
	                                             task_scheduler ts ) {
		if( range.empty( ) ) {
			return range.end( );
		}
		auto const ranges = PartitionPolicy{ }( range, ts.size( ) );
		std::vector<Iterator> results( ranges.size( ), range.end( ) );
		auto sem = partition_range_pos(
		  ranges,
		  [&results, cmp]( daw::view<Iterator> rng, size_t n ) {
			  results[n] =
			    std::max_element( rng.cbegin( ),
					                  rng.cend( ),
					                  [cmp]( auto const &lhs, auto const &rhs ) {
				                      return cmp( lhs, rhs );
			                      } );
		  },
		  ts );
		ts.wait_for( sem );
		return *std::max_element( results.cbegin( ),
		                          results.cend( ),
		                          [cmp]( auto const &lhs, auto const &rhs ) {
			                          return cmp( *lhs, *rhs );
		                          } );
	}

	template<typename PartitionPolicy = split_range_t<>, typename Iterator,
	         typename OutputIterator, typename UnaryOperation>
	void parallel_map( daw::view<Iterator> range_in, OutputIterator first_out,
	                   UnaryOperation unary_op, task_scheduler ts ) {

		partition_range<PartitionPolicy>(
		  range_in,
		  [first_in = range_in.begin( ), first_out, unary_op]( Iterator first,
			                                                     Iterator last ) {
			  auto const step = std::distance( first_in, first );
			  daw::exception::dbg_precondition_check( step >= 0 );

			  daw::algorithm::map(
			    first, last, std::next( first_out, step ), unary_op );
		  },
		  ts )
		  .wait( );
	}

	template<typename PartitionPolicy = split_range_t<>, typename Iterator1,
	         typename Iterator2, typename OutputIterator,
	         typename BinaryOperation>
	void parallel_map( daw::view<Iterator1> range_in1,
	                   daw::view<Iterator2> range_in2, OutputIterator first_out,
	                   BinaryOperation binary_op, task_scheduler ts ) {

		partition_range<PartitionPolicy>(
		  range_in1,
		  [first_in1 = range_in1.begin( ),
			 first_out,
			 first_in2 = range_in2.begin( ),
			 binary_op]( Iterator1 first1, Iterator1 last1 ) {
			  auto const step = std::distance( first_in1, first1 );
			  daw::exception::dbg_precondition_check( step >= 0 );
			  auto out_it = std::next( first_out, step );
			  auto in_it2 = std::next( first_in2, step );

			  daw::algorithm::map( first1, last1, in_it2, out_it, binary_op );
		  },
		  ts )
		  .wait( );
	}

	template<typename PartitionPolicy = split_range_t<2>, typename Iterator,
	         typename T, typename MapFunction, typename ReduceFunction>
	[[nodiscard]] auto
	parallel_map_reduce( daw::view<Iterator> range, T const &init,
	                     MapFunction map_function, ReduceFunction reduce_function,
	                     task_scheduler ts ) {
		static_assert( PartitionPolicy::min_range_size >= 2,
		               "Minimum range size must be >= 2" );
		daw::exception::precondition_check( range.size( ) >= 2 );

		using result_t = daw::remove_cvref_t<decltype( reduce_function(
		  map_function( range.front( ) ), map_function( range.front( ) ) ) )>;

		auto const ranges = PartitionPolicy{ }( range, ts.size( ) );
		auto results = std::vector<std::optional<result_t>>( ranges.size( ) );

		auto sem = partition_range_pos(
		  ranges,
		  [&results, map_function, reduce_function]( daw::view<Iterator> rng,
			                                           size_t n ) {
			  result_t result = map_function( rng.pop_front( ) );
			  while( not rng.empty( ) ) {
				  result = reduce_function( result, map_function( rng.pop_front( ) ) );
			  }
			  results[n] = std::move( result );
		  },
		  ts );

		ts.wait_for( sem );
		auto result = reduce_function( map_function( init ), *results[0] );
		for( size_t n = 1; n < ranges.size( ); ++n ) {
			result = reduce_function( result, *results[n] );
		}
		return result;
	}

	template<typename PartitionPolicy = split_range_t<>, typename Iterator,
	         typename OutputIterator, typename BinaryOp>
	void parallel_scan( daw::view<Iterator> range_in,
	                    daw::view<OutputIterator> range_out, BinaryOp &&binary_op,
	                    task_scheduler ts ) {
		{
			daw::exception::precondition_check(
			  range_in.size( ) == range_out.size( ),
			  "Output range must be the same size as input" );
			if( range_in.size( ) == 2 ) {
				range_out.front( ) = range_in.front( );
				return;
			}
		}
		using value_t = daw::remove_cvref_t<decltype( DAW_FWD( binary_op )(
		  range_in.front( ), range_in.front( ) ) )>;

		auto const ranges = PartitionPolicy{ }( range_in, ts.size( ) );
		auto p1_results = std::vector<std::optional<value_t>>( ranges.size( ) );
		auto mut_p1_results = std::vector<daw::spin_lock>( ranges.size( ) );

		auto const add_result = [&]( size_t pos, value_t const &value ) {
			for( size_t n = pos + 1; n < p1_results.size( ); ++n ) {
				std::lock_guard<daw::spin_lock> lck( mut_p1_results[n] );
				if( p1_results[n] ) {
					p1_results[n] = binary_op( *p1_results[n], value );
				} else {
					p1_results[n] = value;
				}
			}
		};
		// Sum each sub range, but complete the first output as it does
		// not have to be scanned twice
		ts.wait_for( partition_range_pos(
		  ranges,
		  [bin_op = daw::mutable_capture( binary_op ),
			 rng_out = daw::mutable_capture( range_out ),
			 add_result]( daw::view<Iterator> rng, size_t n ) {
			  if( n == 0 ) {
				  auto const lst = std::partial_sum(
				    rng.cbegin( ), rng.cend( ), rng_out->begin( ), *bin_op );
				  add_result(
				    n,
				    *std::next( rng_out->begin( ),
						            std::distance( rng_out->begin( ), lst ) - 1 ) );
				  return;
			  }
			  value_t result = rng.pop_front( );
			  result =
			    daw::algorithm::reduce( rng.cbegin( ), rng.cend( ), result, *bin_op );
			  add_result( n, result );
		  },
		  ts ) );

		ts.wait_for( partition_range_pos(
		  ranges,
		  [&p1_results,
			 rng_in = daw::mutable_capture( range_in ),
			 rng_out = daw::mutable_capture( range_out ),
			 bin_op = daw::mutable_capture( binary_op )](
		    daw::view<Iterator> cur_range, size_t n ) {
			  auto out_pos =
			    std::next( rng_out->begin( ),
					           std::distance( rng_in->begin( ), cur_range.begin( ) ) );

			  auto sum = ( *bin_op )( *p1_results[n], cur_range.pop_front( ) );

			  *( out_pos++ ) = sum;
			  while( not cur_range.empty( ) ) {
				  sum = ( *bin_op )( sum, cur_range.pop_front( ) );
				  *out_pos = sum;
				  ++out_pos;
			  }
		  },
		  ts,
		  1 ) );
	}

	template<typename PartitionPolicy = split_range_t<>, typename Iterator,
	         typename UnaryPredicate>
	[[nodiscard]] Iterator parallel_find_if( daw::view<Iterator> range_in,
	                                         UnaryPredicate &&pred,
	                                         task_scheduler ts ) {

		auto const ranges = PartitionPolicy{ }( range_in, ts.size( ) );
		auto results = std::vector<std::optional<Iterator>>( ranges.size( ) );

		ts.wait_for( partition_range_pos(
		  ranges,
		  [&results, pred]( daw::view<Iterator> range, size_t pos ) {
			  auto it = std::find_if( range.begin( ), range.end( ), pred );
			  if( it != range.end( ) ) {
				  results[pos] = it;
			  }
		  },
		  ts ) );

		for( auto const &it : results ) {
			if( it ) {
				return *it;
			}
		}
		return range_in.end( );
	}

	template<typename PartitionPolicy = split_range_t<>, typename Iterator1,
	         typename Iterator2, typename BinaryPredicate>
	[[nodiscard]] bool parallel_equal( Iterator1 first1, Iterator1 last1,
	                                   Iterator2 first2, Iterator2 last2,
	                                   BinaryPredicate pred, task_scheduler ts ) {
		if( std::distance( first1, last1 ) != std::distance( first2, last2 ) ) {
			return false;
		}
		auto const ranges1 = PartitionPolicy{ }( first1, last1, ts.size( ) );
		auto const ranges2 = PartitionPolicy{ }( first2, last2, ts.size( ) );

		std::atomic_char all_equal = static_cast<char>( true );

		ts.wait_for( partition_range_pos(
		  ranges1,
		  [&ranges2, pred, &all_equal]( daw::view<Iterator1> range1, size_t pos ) {
			  auto range2 = ranges2[pos];
			  all_equal &= std::equal( range1.cbegin( ),
				                         range1.cend( ),
				                         range2.cbegin( ),
				                         range2.cend( ),
				                         pred );
		  },
		  ts ) );
		return static_cast<bool>( all_equal );
	}

	template<typename PartitionPolicy = split_range_t<2>, typename RandomIterator,
	         typename UnaryPredicate>
	[[nodiscard]] auto parallel_count( daw::view<RandomIterator> range_in,
	                                   UnaryPredicate pred, task_scheduler ts ) {
		static_assert( PartitionPolicy::min_range_size >= 2,
		               "Minimum range size must be >= 2" );
		daw::exception::daw_throw_on_false( range_in.size( ) >= 2,
		                                    "Must be at least 2 items in range" );

		using result_t =
		  decltype( std::count_if( range_in.begin( ), range_in.end( ), pred ) );

		if( range_in.size( ) < PartitionPolicy::min_range_size ) {
			return std::count_if( range_in.begin( ), range_in.end( ), pred );
		}
		auto const ranges = PartitionPolicy{ }( range_in, ts.size( ) );
		std::vector<result_t> results( ranges.size( ), 0 );

		auto sem = partition_range_pos(
		  ranges,
		  [&results, pred]( daw::view<RandomIterator> range, size_t n ) {
			  results[n] = std::count_if( range.cbegin( ), range.cend( ), pred );
		  },
		  ts );

		ts.wait_for( sem );
		return std::accumulate(
		  results.cbegin( ), results.cend( ), static_cast<result_t>( 0 ) );
	}
} // namespace daw::algorithm::parallel::impl
