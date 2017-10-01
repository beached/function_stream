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

#include "algorithms_impl.h"

namespace daw {
	namespace algorithm {
		namespace parallel {
			template<typename RandomIterator, typename Func>
			void for_each( RandomIterator const first, RandomIterator const last, Func func,
			               task_scheduler ts = get_task_scheduler( ) ) {
				impl::parallel_for_each( first, last, func, std::move( ts ) );
			}

			template<typename Iterator, typename Func>
			void for_each_n( Iterator first, size_t N, Func func, task_scheduler ts = get_task_scheduler( ) ) {
				impl::parallel_for_each( first, first + N, func, std::move( ts ) );
			}

			template<typename Iterator, typename Func>
			void for_each_index( Iterator first, Iterator last, Func func, task_scheduler ts = get_task_scheduler( ) ) {
				impl::parallel_for_each_index( first, last, func, std::move( ts ) );
			}

			template<typename Iterator, typename T>
			void fill( Iterator first, Iterator last, T const &value, task_scheduler ts = get_task_scheduler( ) ) {
				impl::parallel_for_each( first, last, [&value]( auto &item ) { item = value; }, std::move( ts ) );
			}

			template<typename Iterator,
			         typename LessCompare = std::less<typename std::iterator_traits<Iterator>::value_type>>
			void sort( Iterator first, Iterator last, task_scheduler ts = get_task_scheduler( ),
			           LessCompare compare = LessCompare{} ) {
				impl::parallel_sort( first, last,
				                     []( Iterator f, Iterator l, LessCompare cmp ) { std::sort( f, l, cmp ); },
				                     std::move( compare ), std::move( ts ) );
			}

			template<typename Iterator,
			         typename LessCompare = std::less<typename std::iterator_traits<Iterator>::value_type>>
			void stable_sort( Iterator first, Iterator last, task_scheduler ts = get_task_scheduler( ),
			                  LessCompare compare = LessCompare{} ) {
				impl::parallel_sort( first, last,
				                     []( Iterator f, Iterator l, LessCompare cmp ) { std::stable_sort( f, l, cmp ); },
				                     std::move( compare ), std::move( ts ) );
			}

			template<typename T, typename Iterator, typename BinaryOp>
			auto reduce( Iterator first, Iterator last, T init, BinaryOp binary_op,
			             task_scheduler ts = get_task_scheduler( ) ) {
				return impl::parallel_reduce( first, last, std::move( init ), binary_op, std::move( ts ) );
			}

			template<typename T, typename Iterator>
			auto reduce( Iterator first, Iterator last, T init, task_scheduler ts = get_task_scheduler( ) ) {
				using value_type = typename std::iterator_traits<Iterator>::value_type;
				return ::daw::algorithm::parallel::reduce(
				    first, last, std::move( init ),
				    []( auto const &lhs, auto const &rhs ) -> value_type { return lhs + rhs; }, std::move( ts ) );
			}

			template<typename Iterator>
			auto reduce( Iterator first, Iterator last, task_scheduler ts = get_task_scheduler( ) ) {
				using value_type = typename std::iterator_traits<Iterator>::value_type;
				return ::daw::algorithm::parallel::reduce( first, last, value_type{}, std::move( ts ) );
			}

			template<typename Iterator,
			         typename LessCompare = std::less<std::decay_t<decltype( *std::declval<Iterator>( ) )>>>
			auto min_element( Iterator first, Iterator last, task_scheduler ts = get_task_scheduler( ),
			                  LessCompare compare = LessCompare{} ) {
				return impl::parallel_min_element( first, last, compare, std::move( ts ) );
			}

			template<typename Iterator,
			         typename LessCompare = std::less<std::decay_t<decltype( *std::declval<Iterator>( ) )>>>
			auto max_element( Iterator first, Iterator const last, task_scheduler ts = get_task_scheduler( ),
			                  LessCompare compare = LessCompare{} ) {
				return impl::parallel_max_element( first, last, compare, std::move( ts ) );
			}

			template<typename Iterator, typename OutputIterator, typename UnaryOperation>
			void transform( Iterator first1, Iterator const last1, OutputIterator first2, UnaryOperation unary_op,
			                task_scheduler ts = get_task_scheduler( ) ) {
				impl::parallel_map( first1, last1, first2, unary_op, std::move( ts ) );
			}

			template<typename Iterator, typename UnaryOperation>
			void transform( Iterator first, Iterator last, UnaryOperation unary_op,
			                task_scheduler ts = get_task_scheduler( ) ) {
				impl::parallel_map( first, last, first, unary_op, std::move( ts ) );
			}

			template<typename Iterator, typename MapFunction, typename ReduceFunction>
			auto map_reduce( Iterator first, Iterator last, MapFunction map_function, ReduceFunction reduce_function,
			                 task_scheduler ts = get_task_scheduler( ) ) {
				auto it_init = first;
				std::advance( first, 1 );
				return impl::parallel_map_reduce( first, last, *it_init, map_function, reduce_function,
				                                  std::move( ts ) );
			}

			/// @brief Perform MapReduce on range and return result
			/// @tparam Iterator Type of Range Iterators
			/// @tparam T Type of initial value
			/// @tparam MapFunction Function that maps a->a'
			/// @tparam ReduceFunction Function that takes to items in range and returns 1
			/// @param first Beginning of range
			/// @param last End of range
			/// @param init initial value to supply map/reduce
			/// @param map_function unary function that maps source value to argument of reduce_function
			/// @param reduce_function binary function that maps results of map_function to resulting value
			/// @return Value from reduce function after range is of size 1
			template<typename Iterator, typename T, typename MapFunction, typename ReduceFunction>
			auto map_reduce( Iterator first, Iterator last, T const &init, MapFunction map_function,
			                 ReduceFunction reduce_function, task_scheduler ts = get_task_scheduler( ) ) {
				auto it_init = first;
				std::advance( first, 1 );
				return impl::parallel_map_reduce( first, last, *it_init, map_function, reduce_function,
				                                  std::move( ts ) );
			}

			template<typename Iterator, typename OutputIterator, typename BinaryOp>
			void scan( Iterator first, Iterator last, OutputIterator first_out, OutputIterator last_out,
			           BinaryOp binary_op, task_scheduler ts = get_task_scheduler( ) ) {
				impl::parallel_scan( first, last, first_out, last_out, binary_op, std::move( ts ) );
			}

			template<typename Iterator, typename OutputIterator, typename BinaryOp>
			void scan( Iterator first, Iterator last, BinaryOp binary_op, task_scheduler ts = get_task_scheduler( ) ) {
				impl::parallel_scan( first, last, first, last, binary_op, std::move( ts ) );
			}

			template<typename Iterator, typename UnaryPredicate>
			Iterator find_if( Iterator first, Iterator last, UnaryPredicate pred,
			                           task_scheduler ts = get_task_scheduler( ) ) {
				return impl::parallel_find_if( first, last, pred, std::move( ts ) );
			}

			template<typename Iterator1, typename Iterator2, typename BinaryPredicate>
			bool equal( Iterator1 first1, Iterator1 last1, Iterator2 first2, Iterator2 last2, BinaryPredicate pred,
			            task_scheduler ts = get_task_scheduler( ) ) {

				return impl::parallel_equal( first1, last1, first2, last2, pred, std::move( ts ) );
			}

			template<typename Iterator1, typename Iterator2, typename BinaryPredicate>
			bool equal( Iterator1 first1, Iterator1 last1, Iterator2 first2, Iterator2 last2,
			            task_scheduler ts = get_task_scheduler( ) ) {

				auto const pred = []( auto const & lhs, auto const & rhs ) {
					return lhs == rhs;
				};
				return impl::parallel_equal( first1, last1, first2, last2, pred, std::move( ts ) );
			}

		} // namespace parallel
	}     // namespace algorithm
} // namespace daw

