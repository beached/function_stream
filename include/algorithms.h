// The MIT License (MIT)
//
// Copyright (c) 2016-2018 Darrell Wright
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

#include <daw/cpp_17.h>

#include "algorithms_impl.h"
#include "concept_checks.h"

namespace daw {
	namespace algorithm {
		namespace parallel {
			template<typename RandomIterator, typename UnaryOperation>
			void for_each( RandomIterator first, RandomIterator last, UnaryOperation unary_op,
			               task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
				               "UnaryOperation passed to for_each must accept the value referenced by first. e.g "
				               "unary_op( *first ) must be valid" );

				impl::parallel_for_each( first, last, unary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename UnaryOperation>
			void for_each_n( RandomIterator first, size_t N, UnaryOperation unary_op,
			                 task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
				               "UnaryOperation passed to for_each_n must accept the value referenced by first. e.g "
				               "unary_op( *first ) must be valid" );

				auto const last = std::next( first, static_cast<intmax_t>( N ) );
				impl::parallel_for_each( first, last, unary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename UnaryOperation>
			void for_each_index( RandomIterator first, RandomIterator last, UnaryOperation indexed_op,
			                     task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::is_callable_v<UnaryOperation, size_t>,
				               "UnaryOperation passed to "
				               "for_each_index must a size_t argument "
				               "unary_op( (size_t)5 ) must be valid" );

				impl::parallel_for_each_index( first, last, indexed_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename T>
			void fill( RandomIterator first, RandomIterator last, T const &value,
			           task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::is_assignable_iterator_v<RandomIterator, T>,
				               "T value must be assignable to the "
				               "dereferenced RandomIterator first. "
				               "e.g. *first = value is valid" );
				impl::parallel_for_each( first, last, [&value]( auto &item ) { item = value; }, std::move( ts ) );
			}

			template<typename RandomIterator,
			         typename Compare = std::less<typename std::iterator_traits<RandomIterator>::value_type>>
			void sort( RandomIterator first, RandomIterator last, task_scheduler ts = get_task_scheduler( ),
			           Compare comp = Compare{} ) {

				static_assert( daw::concept_checks::is_binary_predicate_v<Compare, RandomIterator, RandomIterator>,
				               "Supplied Compare does not satisfy the concept of BinaryPredicate.  See "
				               "http://en.cppreference.com/w/cpp/concept/BinaryPredicate" );

				impl::parallel_sort( first, last,
				                     []( RandomIterator f, RandomIterator l, Compare cmp ) { std::sort( f, l, cmp ); },
				                     std::move( comp ), std::move( ts ) );
			}

			template<typename RandomIterator,
			         typename Compare = std::less<typename std::iterator_traits<RandomIterator>::value_type>>
			void stable_sort( RandomIterator first, RandomIterator last, task_scheduler ts = get_task_scheduler( ),
			                  Compare comp = Compare{} ) {

				static_assert( daw::concept_checks::is_binary_predicate_v<Compare, RandomIterator, RandomIterator>,
				               "Supplied Compare does not satisfy the concept of BinaryPredicate.  See "
				               "http://en.cppreference.com/w/cpp/concept/BinaryPredicate" );

				impl::parallel_sort( first, last,
				                     []( RandomIterator f, RandomIterator l, Compare cmp ) { std::stable_sort( f, l, cmp ); },
				                     std::move( comp ), std::move( ts ) );
			}

			template<typename T, typename RandomIterator, typename BinaryOperation>
			T reduce( RandomIterator first, RandomIterator last, T init, BinaryOperation binary_op,
			          task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_callable_v<BinaryOperation, RandomIterator, RandomIterator>,
				               "BinaryOperation passed to reduce must take two values referenced by first. e.g binary_op( "
				               "*first, *(first+1) ) "
				               "must be valid" );

				static_assert(
				  std::is_convertible<daw::concept_checks::is_callable_t<BinaryOperation, RandomIterator, RandomIterator>,
				                      typename std::iterator_traits<RandomIterator>::value_type>::value,
				  "Result of BinaryOperation must be convertable to type of value referenced by RandomIterator. "
				  "e.g. *first = binary_op( *first, *(first + 1) ) must be valid." );

				return impl::parallel_reduce( first, last, std::move( init ), binary_op, std::move( ts ) );
			}

			template<typename T, typename RandomIterator>
			T reduce( RandomIterator first, RandomIterator last, T init, task_scheduler ts = get_task_scheduler( ) ) {

				using value_type = typename std::iterator_traits<RandomIterator>::value_type;
				return ::daw::algorithm::parallel::reduce(
				  first, last, std::move( init ), []( auto const &lhs, auto const &rhs ) -> value_type { return lhs + rhs; },
				  std::move( ts ) );
			}

			template<typename RandomIterator>
			auto reduce( RandomIterator first, RandomIterator last, task_scheduler ts = get_task_scheduler( ) ) {
				using value_type = typename std::iterator_traits<RandomIterator>::value_type;
				return ::daw::algorithm::parallel::reduce( first, last, value_type{}, std::move( ts ) );
			}

			template<typename RandomIterator,
			         typename Compare = std::less<std::decay_t<decltype( *std::declval<RandomIterator>( ) )>>>
			auto min_element( RandomIterator first, RandomIterator last, task_scheduler ts = get_task_scheduler( ),
			                  Compare comp = Compare{} ) {

				static_assert( daw::concept_checks::is_binary_predicate_v<Compare, RandomIterator, RandomIterator>,
				               "Supplied Compare does not satisfy the concept of BinaryPredicate.  See "
				               "http://en.cppreference.com/w/cpp/concept/BinaryPredicate" );

				return impl::parallel_min_element( first, last, comp, std::move( ts ) );
			}

			template<typename RandomIterator,
			         typename Compare = std::less<std::decay_t<decltype( *std::declval<RandomIterator>( ) )>>>
			auto max_element( RandomIterator first, RandomIterator const last, task_scheduler ts = get_task_scheduler( ),
			                  Compare comp = Compare{} ) {

				static_assert( daw::concept_checks::is_binary_predicate_v<Compare, RandomIterator, RandomIterator>,
				               "Supplied Compare does not satisfy the concept of BinaryPredicate.  See "
				               "http://en.cppreference.com/w/cpp/concept/BinaryPredicate" );

				return impl::parallel_max_element( first, last, comp, std::move( ts ) );
			}

			template<typename RandomIterator, typename RandomOutputIterator, typename UnaryOperation>
			void transform( RandomIterator first, RandomIterator const last, RandomOutputIterator first_out,
			                UnaryOperation unary_op, task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
				               "UnaryOperation passed to transform must accept the value referenced by first. e.g "
				               "unary_op( *first ) must be valid" );
				static_assert(
				  daw::is_assignable_iterator_v<RandomOutputIterator,
				                                daw::concept_checks::is_callable_t<UnaryOperation, RandomIterator>>,
				  "The result of the UnaryOperation must be assignable to the dereferenced "
				  "RandomOutputIterator. e.g. *first_out = unary_op( *first ) must be valid" );

				impl::parallel_map( first, last, first_out, unary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename UnaryOperation>
			void transform( RandomIterator first, RandomIterator last, UnaryOperation unary_op,
			                task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
				               "UnaryOperation passed to transform must accept the value referenced by first. e.g "
				               "unary_op( *first ) must be valid" );
				static_assert(
				  daw::is_assignable_iterator_v<RandomIterator,
				                                daw::concept_checks::is_callable_t<UnaryOperation, RandomIterator>>,
				  "The result of the UnaryOperation must be assignable to the dereferenced "
				  "RandomIterator. e.g. *first_out = unary_op( *first ) must be valid" );

				impl::parallel_map( first, last, first, unary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename UnaryOperation, typename BinaryOperation>
			auto map_reduce( RandomIterator first, RandomIterator last, UnaryOperation map_function,
			                 BinaryOperation reduce_function, task_scheduler ts = get_task_scheduler( ) ) {

				static_assert(
				  daw::concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
				  "UnaryOperation map_function passed to map_reduce must accept the value referenced by first. e.g "
				  "map_function( *first ) must be valid" );

				using transform_result_t = daw::concept_checks::is_callable_t<UnaryOperation, RandomIterator>;
				static_assert(
				  daw::is_callable_v<BinaryOperation, transform_result_t, transform_result_t>,
				  "BinaryOperation reduce_function passed to map_reduce must accept the result of the "
				  "transform_function for each arg. e.g "
				  "reduce_function( tranfom_function( *first ), transform_function( *(first + 1) ) ) must be valid" );

				auto it_init = first;
				std::advance( first, 1 );
				return impl::parallel_map_reduce( first, last, *it_init, map_function, reduce_function, std::move( ts ) );
			}

			/// @brief Perform MapReduce on range and return result
			/// @tparam RandomIterator Type of Range RandomIterators
			/// @tparam T Type of initial value
			/// @tparam UnaryOperation Function that maps a->a'
			/// @tparam BinaryOperation Function that takes to items in range and returns 1
			/// @param first Beginning of range
			/// @param last End of range
			/// @param init initial value to supply map/reduce
			/// @param map_function unary function that maps source value to argument of reduce_function
			/// @param reduce_function binary function that maps results of map_function to resulting value
			/// @return Value from reduce function after range is of size 1
			template<typename RandomIterator, typename T, typename UnaryOperation, typename BinaryOperation>
			auto map_reduce( RandomIterator first, RandomIterator last, T const &init, UnaryOperation map_function,
			                 BinaryOperation reduce_function, task_scheduler ts = get_task_scheduler( ) ) {

				static_assert(
				  daw::concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
				  "UnaryOperation map_function passed to map_reduce must accept the value referenced by first. e.g "
				  "map_function( *first ) must be valid" );

				static_assert( daw::concept_checks::is_callable_v<UnaryOperation, T>,
				               "UnaryOperation map_function passed to map_reduce must accept the init value of type T. e.g "
				               "map_function( value ) must be valid" );

				using transform_result_t = daw::concept_checks::is_callable_t<UnaryOperation, RandomIterator>;
				static_assert(
				  daw::concept_checks::is_callable_v<BinaryOperation, transform_result_t, transform_result_t>,
				  "BinaryOperation reduce_function passed to map_reduce must accept the result of the "
				  "transform_function for each arg. e.g "
				  "reduce_function( tranfom_function( *first ), transform_function( *(first + 1) ) ) must be valid" );

				auto it_init = first;
				std::advance( first, 1 );
				return impl::parallel_map_reduce( first, last, *it_init, map_function, reduce_function, std::move( ts ) );
			}

			template<typename RandomIterator, typename RandomOutputIterator, typename BinaryOperation>
			void scan( RandomIterator first, RandomIterator last, RandomOutputIterator first_out,
			           RandomOutputIterator last_out, BinaryOperation binary_op, task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_callable_v<BinaryOperation, RandomIterator, RandomIterator>,
				               "BinaryOperation passed to scan must take two values referenced by first. e.g "
				               "binary_op( *first, *(first+1) ) must be valid" );

				static_assert(
				  daw::is_assignable_iterator_v<
				    RandomOutputIterator, daw::concept_checks::is_callable_t<BinaryOperation, RandomIterator, RandomIterator>>,
				  "The result of the BinaryOperation must be assignable to the dereferenced "
				  "RandomOutputIterator. e.g. *first_out = unary_op( *first, *(fist + 1) ) must be valid" );

				impl::parallel_scan( first, last, first_out, last_out, binary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename RandomOutputIterator, typename BinaryOperation>
			void scan( RandomIterator first, RandomIterator last, BinaryOperation binary_op,
			           task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_callable_v<BinaryOperation, RandomIterator, RandomIterator>,
				               "BinaryOperation passed to scan must take two values referenced by first. e.g "
				               "binary_op( *first, *(first+1) ) must be valid" );

				static_assert(
				  daw::is_assignable_iterator_v<
				    RandomIterator, daw::concept_checks::is_callable_t<BinaryOperation, RandomIterator, RandomIterator>>,
				  "The result of the BinaryOperation must be assignable to the dereferenced "
				  "RandomIterator. e.g. *first = unary_op( *first, *(fist + 1) ) must be valid" );

				impl::parallel_scan( first, last, first, last, binary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename UnaryPredicate>
			RandomIterator find_if( RandomIterator first, RandomIterator last, UnaryPredicate pred,
			                        task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_unary_predicate_v<UnaryPredicate, RandomIterator>,
				               "Supplied UnaryPredicate pred does not satisfy the concept of UnaryPredicate.  See "
				               "http://en.cppreference.com/w/cpp/concept/Predicate" );

				return impl::parallel_find_if( first, last, pred, std::move( ts ) );
			}

			template<typename RandomIterator1, typename RandomIterator2, typename BinaryPredicate>
			bool equal( RandomIterator1 first1, RandomIterator1 last1, RandomIterator2 first2, RandomIterator2 last2,
			            BinaryPredicate pred, task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_binary_predicate_v<BinaryPredicate, RandomIterator1, RandomIterator2>,
				               "Supplied BinaryPredicate does not satisfy the concept of BinaryPredicate.  See "
				               "http://en.cppreference.com/w/cpp/concept/BinaryPredicate" );
				return impl::parallel_equal( first1, last1, first2, last2, pred, std::move( ts ) );
			}

			template<typename RandomIterator1, typename RandomIterator2>
			bool equal( RandomIterator1 first1, RandomIterator1 last1, RandomIterator2 first2, RandomIterator2 last2,
			            task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_equality_comparable_v<RandomIterator1, RandomIterator2>,
				               "Dereferenced RandomIterator1 and RandomIterator2 must be equality comparable e.g. "
				               "*first1 == *first2 must be valid" );

				auto const pred = []( auto const &lhs, auto const &rhs ) { return lhs == rhs; };
				return impl::parallel_equal( first1, last1, first2, last2, pred, std::move( ts ) );
			}

			template<typename RandomIterator, typename UnaryPredicate>
			auto count_if( RandomIterator first, RandomIterator last, UnaryPredicate pred,
			               task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::concept_checks::is_unary_predicate_v<UnaryPredicate, RandomIterator>,
				               "Supplied UnaryPredicate pred does not satisfy the concept of UnaryPredicate.  See "
				               "http://en.cppreference.com/w/cpp/concept/Predicate" );

				return impl::parallel_count( first, last, pred, std::move( ts ) );
			}

			template<typename RandomIterator, typename T>
			auto count( RandomIterator first, RandomIterator last, T const &value,
			            task_scheduler ts = get_task_scheduler( ) ) {

				return impl::parallel_count( first, last, [&value]( auto const &rhs ) { return value == rhs; },
				                             std::move( ts ) );
			}

			template<size_t minimum_size = 1>
			using default_range_splitter = impl::split_range_t<minimum_size>;

			template<typename PartitionPolicy = default_range_splitter<>, typename RandomIterator, typename Function>
			void chunked_for_each( RandomIterator first, RandomIterator last, Function func,
			                       task_scheduler ts = get_task_scheduler( ) ) {
				auto ranges = PartitionPolicy{}( first, last, ts.size( ) );
				impl::partition_range( ranges, std::move( func ), std::move( ts ) ).wait( );
			}

			template<typename PartitionPolicy = default_range_splitter<>, typename RandomIterator, typename Function>
			void chunked_for_each_pos( RandomIterator first, RandomIterator last, Function func,
			                           task_scheduler ts = get_task_scheduler( ) ) {
				auto ranges = PartitionPolicy{}( first, last, ts.size( ) );
				impl::partition_range_pos( ranges, std::move( func ), std::move( ts ) ).wait( );
			}
		} // namespace parallel
	}   // namespace algorithm
} // namespace daw
