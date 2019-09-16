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

#include <algorithm>
#include <iterator>
#include <numeric>
#include <optional>

#include <daw/daw_algorithm.h>
#include <daw/daw_view.h>
#include <daw/parallel/daw_latch.h>
#include <daw/parallel/daw_spin_lock.h>

#include "../future_result.h"
#include "../task_scheduler.h"

namespace daw::algorithm::parallel::impl {
	template<size_t MinRangeSize, typename Iterator>
	[[nodiscard]] auto get_part_info( daw::view<Iterator> rng,
	                                  size_t max_parts ) noexcept {

		static_assert( MinRangeSize != 0, "Minimum range size must be > 0" );

		struct {
			size_t size = 0;
			size_t count = 0;
		} result{};

		result.size = [&]( ) {
			auto r = rng.size( ) / max_parts;
			if( r < MinRangeSize ) {
				r = MinRangeSize;
			}
			return r;
		}( );
		result.count = rng.size( ) / result.size;
		if( result.count == 0 )
			if( result.count == 0 ) {
				result.count = 1;
			}
		if( rng.size( ) > ( result.count * result.size ) ) {
			++result.count;
		}
		return result;
	}

	template<size_t MinRangeSize = 1>
	struct [[nodiscard]] split_range_t {
		static_assert( MinRangeSize != 0, "Minimum range size must be > 0" );
		static constexpr size_t min_range_size = MinRangeSize;

		template<typename Iterator>
		[[nodiscard]] std::vector<daw::view<Iterator>> operator( )(
		  Iterator first, Iterator last, size_t const max_parts ) const {

			daw::view rng( first, last );

			std::vector<daw::view<Iterator>> results{};

			auto const part_info = get_part_info<min_range_size>( rng, max_parts );

			while( not rng.empty( ) ) {
				results.push_back( rng.pop_front( part_info.size ) );
			}
			return results;
		}
		template<typename Iterator>
		[[nodiscard]] std::vector<daw::view<Iterator>> operator( )(
		  daw::view<Iterator> rng, size_t const max_parts ) const {
			return operator( )( rng.begin( ), rng.end( ), max_parts );
		}
	};

	template<typename RandomIterator, typename Func>
	[[nodiscard]] daw::shared_latch
	partition_range( std::vector<daw::view<RandomIterator>> ranges, Func &&func,
	                 task_scheduler ts ) {
		auto sem = daw::shared_latch( ranges.size( ) );
		for( auto rng : ranges ) {
			if( not schedule_task(
			      sem,
			      [func = daw::mutable_capture( func ), rng = daw::move( rng )]( ) {
				      ( *func )( rng );
			      },
			      ts ) ) {

				throw ::daw::unable_to_add_task_exception{};
			}
		}
		return sem;
	}

	template<typename RandomIterator, typename Func>
	[[nodiscard]] daw::shared_latch
	partition_range_pos( std::vector<daw::view<RandomIterator>> ranges, Func func,
	                     task_scheduler const &ts, size_t const start_pos = 0 ) {
		auto sem = daw::shared_latch( ranges.size( ) - start_pos );
		for( size_t n = start_pos; n < ranges.size( ); ++n ) {
			if( not schedule_task(
			      sem,
			      [func = daw::mutable_capture( func ), rng = ranges[n], n]( ) {
				      ( *func )( rng, n );
			      },
			      ts ) ) {

				throw ::daw::unable_to_add_task_exception{};
			}
		}
		return sem;
	}

	template<typename PartitionPolicy, typename RandomIterator, typename Func>
	[[nodiscard]] daw::shared_latch
	partition_range( daw::view<RandomIterator> range, Func &&func,
	                 task_scheduler ts ) {
		if( range.empty( ) ) {
			return {};
		}
		auto const ranges = PartitionPolicy{}( range, ts.size( ) );
		auto sem = daw::shared_latch( ranges.size( ) );
		bool status = true;
		for( auto rng : ranges ) {
			status &= schedule_task(
			  sem,
			  [func = daw::mutable_capture( std::forward<Func>( func ) ), rng]( ) {
				  ( *func )( rng.begin( ), rng.end( ) );
			  },
			  ts );
		}
		if( not status ) {
			throw ::daw::unable_to_add_task_exception{};
		}
		return sem;
	}

	template<typename PartitionPolicy = split_range_t<>, typename RandomIterator,
	         typename Func>
	void parallel_for_each( daw::view<RandomIterator> rng, Func &&func,
	                        task_scheduler ts ) {

		static_assert( std::is_invocable_v<Func, decltype( rng.front( ) )> );

		partition_range<PartitionPolicy>(
		  rng,
		  [func = daw::mutable_capture( std::forward<Func>( func ) )](
		    auto &&first, auto &&last ) {
			  while( first != last ) {
				  ( *func )( *first );
				  ++first;
			  }
		  },
		  ts )
		  .wait( );
	}

	template<typename Ranges, typename Func>
	void parallel_for_each( Ranges &ranges, Func func, task_scheduler ts ) {
		partition_range(
		  ranges,
		  [func]( auto f, auto l ) mutable {
			  for( auto it = f; it != l; ++it ) {
				  func( *it );
			  }
		  },
		  ts )
		  .wait( );
	}

	template<typename PartitionPolicy = split_range_t<>, typename RandomIterator,
	         typename Func>
	void parallel_for_each_index( RandomIterator first, RandomIterator last,
	                              Func func, task_scheduler ts ) {
		auto const ranges = PartitionPolicy{}( first, last, ts.size( ) );
		auto sem = daw::shared_latch( ranges.size( ) );
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

	template<typename Iterator, typename Function>
	void merge_reduce_range( std::vector<daw::view<Iterator>> ranges,
	                         Function func, task_scheduler ts ) {
		while( ranges.size( ) > 1 ) {
			auto const count =
			  ( ranges.size( ) % 2 == 0 ? ranges.size( ) : ranges.size( ) - 1 );
			std::vector<view<Iterator>> next_ranges;
			next_ranges.reserve( count );
			auto sem = daw::latch( count / 2 );
			for( size_t n = 1; n < count; n += 2 ) {
				next_ranges.push_back( {ranges[n - 1].begin( ), ranges[n].end( )} );
			}
			if( count != ranges.size( ) ) {
				next_ranges.push_back( ranges.back( ) );
			}
			for( size_t n = 1; n < count; n += 2 ) {
				if( not ts.add_task(
				      [func = mutable_capture( func ), &ranges, n, &sem]( ) {
					      ( *func )( ranges[n - 1].begin( ), ranges[n].begin( ),
					                 ranges[n].end( ) );
					      sem.notify( );
				      } ) ) {

					throw ::daw::unable_to_add_task_exception{};
				}
			}
			sem.wait( );
			std::swap( ranges, next_ranges );
		}
	}

	template<typename Compare>
	struct parallel_sort_merger {
		Compare cmp;

		template<typename Iterator>
		constexpr ::daw::view<Iterator>
		operator( )( daw::view<Iterator> rng_left,
		             daw::view<Iterator> rng_right ) const {
			daw::exception::dbg_precondition_check(
			  rng_left.end( ) == rng_right.begin( ), "Ranges must be contigous" );

			std::inplace_merge( rng_left.begin( ), rng_left.end( ), rng_right.end( ),
			                    cmp );

			return daw::view<Iterator>( rng_left.begin( ), rng_right.end( ) );
		}
	};

	struct sorter {
		template<typename RandomIterator, typename Compare>
		constexpr void operator( )( RandomIterator &&first, RandomIterator &&last,
		                            Compare &&cmp ) const {
			::std::sort( ::std::forward<RandomIterator>( first ),
			             ::std::forward<RandomIterator>( last ),
			             std::forward<Compare>( cmp ) );
		}
	};

	struct stable_sorter {
		template<typename RandomIterator, typename Compare>
		constexpr void operator( )( RandomIterator &&first, RandomIterator &&last,
		                            Compare &&cmp ) const {
			::std::stable_sort( ::std::forward<RandomIterator>( first ),
			                    ::std::forward<RandomIterator>( last ),
			                    std::forward<Compare>( cmp ) );
		}
	};

	template<typename Compare>
	parallel_sort_merger( Compare cmp )->parallel_sort_merger<Compare>;

	template<typename PartitionPolicy = split_range_t<>, typename Iterator,
	         typename Sort, typename Compare>
	void parallel_sort( daw::view<Iterator> range, Sort &&srt, Compare &&cmp,
	                    task_scheduler ts ) {
		if( PartitionPolicy::min_range_size > range.size( ) ) {
			srt( range.begin( ), range.end( ), cmp );
			return;
		}
		auto ranges = PartitionPolicy( )( range, ts.size( )*2 );

		auto sorters = std::vector<future_result_t<daw::view<Iterator>>>( );
		sorters.reserve( ranges.size( ) );

		auto const sort_fn = [cmp = mutable_capture( cmp ),
		                      srt = mutable_capture( std::forward<Sort>( srt ) )](
		                       daw::view<Iterator> r ) {
			( *srt )( r.begin( ), r.end( ), *cmp );
			return r;
		};

		daw::algorithm::transform(
		  ranges.begin( ), ranges.end( ), std::back_inserter( sorters ),
		  [ts = daw::mutable_capture( ts ), sort_fn]( daw::view<Iterator> rng ) {
			  return make_future_result( *ts, sort_fn, rng );
		  } );

		ts.wait_for( reduce_futures( sorters.begin( ), sorters.end( ),
		                             parallel_sort_merger{cmp} ) );
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
		auto const ranges = PartitionPolicy{}( range, ts.size( ) );
		auto results = std::vector<std::optional<T>>( ranges.size( ) );
		auto sem = partition_range_pos(
		  ranges,
		  [&results, binary_op]( daw::view<Iterator> rng, size_t n ) {
			  results[n] = std::accumulate( std::next( rng.cbegin( ) ), rng.cend( ),
			                                rng.front( ), binary_op );
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

	template<typename Type1, typename Type2, typename Compare>
	[[nodiscard]] Type1 compare_value( Type1 const &lhs, Type2 const &rhs,
	                                   Compare cmp ) {
		if( cmp( lhs, rhs ) ) {
			return lhs;
		}
		return rhs;
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
		auto const ranges = PartitionPolicy{}( range, ts.size( ) );
		auto results = std::vector<Iterator>( ranges.size( ), range.end( ) );
		auto sem =
		  partition_range_pos( ranges, min_element_worker{results, cmp}, ts );
		ts.wait_for( sem );

		return *std::min_element(
		  results.cbegin( ), results.cend( ),
		  [cmp]( auto const &lhs, auto const &rhs ) { return cmp( *lhs, *rhs ); } );
	}

	template<typename PartitionPolicy = split_range_t<>, typename Iterator,
	         typename Compare>
	[[nodiscard]] Iterator parallel_max_element( daw::view<Iterator> range,
	                                             Compare cmp,
	                                             task_scheduler ts ) {
		if( range.empty( ) ) {
			return range.end( );
		}
		auto const ranges = PartitionPolicy{}( range, ts.size( ) );
		std::vector<Iterator> results( ranges.size( ), range.end( ) );
		auto sem = partition_range_pos(
		  ranges,
		  [&results, cmp]( daw::view<Iterator> rng, size_t n ) {
			  results[n] =
			    std::max_element( rng.cbegin( ), rng.cend( ),
			                      [cmp]( auto const &lhs, auto const &rhs ) {
				                      return cmp( lhs, rhs );
			                      } );
		  },
		  ts );
		ts.wait_for( sem );
		return *std::max_element(
		  results.cbegin( ), results.cend( ),
		  [cmp]( auto const &lhs, auto const &rhs ) { return cmp( *lhs, *rhs ); } );
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

			  daw::algorithm::map( first, last, std::next( first_out, step ),
			                       unary_op );
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
		  [first_in1 = range_in1.begin( ), first_out,
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

		auto const ranges = PartitionPolicy{}( range, ts.size( ) );
		auto results = std::vector<std::optional<result_t>>( ranges.size( ) );

		auto sem = partition_range_pos(
		  ranges,
		  [&results, map_function, reduce_function]( daw::view<Iterator> rng,
		                                             size_t n ) {
			  result_t result = map_function( rng.pop_front( ) );
			  while( not rng.empty( ) ) {
				  result = reduce_function( result, map_function( rng.pop_front( ) ) );
			  }
			  results[n] = daw::move( result );
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
		using value_t = daw::remove_cvref_t<decltype( std::forward<BinaryOp>(
		  binary_op )( range_in.front( ), range_in.front( ) ) )>;

		auto const ranges = PartitionPolicy{}( range_in, ts.size( ) );
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
		  [binary_op = daw::mutable_capture( binary_op ),
		   range_out = daw::mutable_capture( range_out ),
		   add_result]( daw::view<Iterator> rng, size_t n ) {
			  if( n == 0 ) {
				  auto const lst = std::partial_sum( rng.cbegin( ), rng.cend( ),
				                                     range_out->begin( ), *binary_op );
				  add_result(
				    n, *std::next( range_out->begin( ),
				                   std::distance( range_out->begin( ), lst ) - 1 ) );
				  return;
			  }
			  value_t result = rng.pop_front( );
			  result = daw::algorithm::reduce( rng.cbegin( ), rng.cend( ), result,
			                                   *binary_op );
			  add_result( n, result );
		  },
		  ts ) );

		ts.wait_for( partition_range_pos(
		  ranges,
		  [&p1_results, range_in = daw::mutable_capture( range_in ),
		   range_out = daw::mutable_capture( range_out ),
		   binary_op = daw::mutable_capture( binary_op )](
		    daw::view<Iterator> cur_range, size_t n ) {
			  auto out_pos =
			    std::next( range_out->begin( ),
			               std::distance( range_in->begin( ), cur_range.begin( ) ) );

			  auto sum = ( *binary_op )( *p1_results[n], cur_range.pop_front( ) );

			  *( out_pos++ ) = sum;
			  while( not cur_range.empty( ) ) {
				  sum = ( *binary_op )( sum, cur_range.pop_front( ) );
				  *out_pos = sum;
				  ++out_pos;
			  }
		  },
		  ts, 1 ) );
	}

	template<typename PartitionPolicy = split_range_t<>, typename Iterator,
	         typename UnaryPredicate>
	[[nodiscard]] Iterator parallel_find_if( daw::view<Iterator> range_in,
	                                         UnaryPredicate &&pred,
	                                         task_scheduler ts ) {

		auto const ranges = PartitionPolicy{}( range_in, ts.size( ) );
		auto results =
		  std::vector<std::optional<Iterator>>( ranges.size( ), range_in.end( ) );

		std::atomic<size_t> has_found{std::numeric_limits<size_t>::max( )};
		daw::spin_lock mut_found;
		ts.wait_for( partition_range_pos(
		  ranges,
		  [&results, pred, &has_found, &mut_found]( daw::view<Iterator> range,
		                                            size_t pos ) {
			  size_t const stride = std::thread::hardware_concurrency( ) * 100;
			  size_t m = 0;
			  for( size_t n = 0; n < range.size( ); n += stride ) {
				  auto const last_val =
				    ( n + stride < range.size( ) ) ? n + stride : range.size( );
				  for( m = n; m < last_val; ++m ) {
					  if( pred( range[m] ) ) {
						  results[pos] = std::next(
						    range.begin( ),
						    static_cast<
						      typename std::iterator_traits<Iterator>::difference_type>(
						      m ) );
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
		auto const ranges1 = PartitionPolicy{}( first1, last1, ts.size( ) );
		auto const ranges2 = PartitionPolicy{}( first2, last2, ts.size( ) );
		std::atomic_bool is_equal{true};
		ts.wait_for( partition_range_pos(
		  ranges1,
		  [pred, &is_equal, &ranges2]( daw::view<Iterator1> range1, size_t pos ) {
			  auto range2 = ranges2[pos];

			  size_t const stride = daw::max(
			    static_cast<size_t>( std::thread::hardware_concurrency( ) ) * 100ULL,
			    static_cast<size_t>( PartitionPolicy::min_range_size ) );
			  size_t m = 0;
			  for( size_t n = 0;
			       n < range1.size( ) && is_equal.load( std::memory_order_relaxed );
			       n += stride ) {
				  auto const last_val = [&]( ) {
					  auto const lv_res = n + stride;
					  if( lv_res > range1.size( ) ) {
						  return range1.size( );
					  }
					  return lv_res;
				  }( );
				  for( m = n; m < last_val; ++m ) {
					  if( not pred( range1[m], range2[m] ) ) {
						  is_equal.store( false, std::memory_order_relaxed );
						  return;
					  }
				  }
			  }
		  },
		  ts ) );
		return is_equal.load( std::memory_order_relaxed );
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
		auto const ranges = PartitionPolicy{}( range_in, ts.size( ) );
		std::vector<result_t> results( ranges.size( ), 0 );

		auto sem = partition_range_pos(
		  ranges,
		  [&results, pred]( daw::view<RandomIterator> range, size_t n ) {
			  results[n] = std::count_if( range.cbegin( ), range.cend( ), pred );
		  },
		  ts );

		ts.wait_for( sem );
		return std::accumulate( results.cbegin( ), results.cend( ),
		                        static_cast<result_t>( 0 ) );
	}
} // namespace daw::algorithm::parallel::impl
