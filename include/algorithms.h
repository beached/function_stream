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

#include <daw/cpp_17.h>

#include "algorithms_impl.h"

namespace daw {
	namespace algorithm {
		namespace parallel {
			namespace details {
				template<typename Iterator, typename T>
				using unary_op_arg_test = decltype(
				    std::declval<T &>( )( std::declval<typename std::iterator_traits<Iterator>::value_type>( ) ) );

				template<typename Iterator, typename T>
				using binary_op_arg_test = decltype(
				    std::declval<T &>( )( std::declval<typename std::iterator_traits<Iterator>::value_type>( ),
				                          std::declval<typename std::iterator_traits<Iterator>::value_type>( ) ) );

				template<typename T>
				using size_t_arg_test = decltype( std::declval<T &>( )( std::declval<size_t>( ) ) );

				template<typename Value, typename T>
				using reduce_arg_test =
				    decltype( std::declval<T &>( )( std::declval<Value>( ), std::declval<Value>( ) ) );

				template<typename LHS, typename RHS>
				using is_assignable_test = decltype( std::declval<LHS &>( ) = std::declval<RHS &>( ) );
			} // namespace details

			template<typename RandomIterator, typename UnaryOperation>
			void for_each( RandomIterator first, RandomIterator last, UnaryOperation unary_op,
			               task_scheduler ts = get_task_scheduler( ) ) {

				static_assert(
				    daw::is_detected_v<details::unary_op_arg_test, RandomIterator, UnaryOperation>,
				    "UnaryOperation passed to for_each must accept the value referenced by first. e.g unary_op( *first ) "
				    "must be valid" );
				impl::parallel_for_each( first, last, unary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename UnaryOperation>
			void for_each_n( RandomIterator first, size_t N, UnaryOperation unary_op, task_scheduler ts = get_task_scheduler( ) ) {
				static_assert(
				    daw::is_detected_v<details::unary_op_arg_test, RandomIterator, UnaryOperation>,
				    "UnaryOperation passed to for_each_n must accept the value referenced by first. e.g unary_op( *first ) "
				    "must be valid" );

				impl::parallel_for_each( first, first + N, unary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename IndexedOperation>
			void for_each_index( RandomIterator first, RandomIterator last, IndexedOperation indexed_op, task_scheduler ts = get_task_scheduler( ) ) {
				static_assert(
				    daw::is_detected_v<details::size_t_arg_test, IndexedOperation>,
				    "IndexedOperation passed to for_each_index must accept a size_t as argument. e.g. indexed_op( 5 ) must be valid" );

				impl::parallel_for_each_index( first, last, indexed_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename T>
			void fill( RandomIterator first, RandomIterator last, T const &value, task_scheduler ts = get_task_scheduler( ) ) {
				impl::parallel_for_each( first, last, [&value]( auto &item ) { item = value; }, std::move( ts ) );
			}

			template<typename RandomIterator,
			         typename LessCompare = std::less<typename std::iterator_traits<RandomIterator>::value_type>>
			void sort( RandomIterator first, RandomIterator last, task_scheduler ts = get_task_scheduler( ),
			           LessCompare compare = LessCompare{} ) {
				using binary_pred_result_t = daw::is_detected<details::binary_op_arg_test, RandomIterator, LessCompare>;
				static_assert( binary_pred_result_t::value,
				               "LessCompare passed to sort must take two values referenced by first. e.g compare( "
				               "*first, *(first+1) ) "
				               "must be valid" );
				static_assert( std::is_convertible<typename binary_pred_result_t::type, bool>::value,
				               "Result of LessCompare compare must be convertible to bool" );

				impl::parallel_sort( first, last,
				                     []( RandomIterator f, RandomIterator l, LessCompare cmp ) { std::sort( f, l, cmp ); },
				                     std::move( compare ), std::move( ts ) );
			}

			template<typename RandomIterator,
			         typename LessCompare = std::less<typename std::iterator_traits<RandomIterator>::value_type>>
			void stable_sort( RandomIterator first, RandomIterator last, task_scheduler ts = get_task_scheduler( ),
			                  LessCompare compare = LessCompare{} ) {
				using binary_pred_result_t = daw::is_detected<details::binary_op_arg_test, RandomIterator, LessCompare>;
				static_assert(
				    binary_pred_result_t::value,
				    "LessCompare passed to stable_sort must take two values referenced by first. e.g compare( "
				    "*first, *(first+1) ) "
				    "must be valid" );
				static_assert( std::is_convertible<typename binary_pred_result_t::type, bool>::value,
				               "Result of LessCompare compare must be convertible to bool" );

				impl::parallel_sort( first, last,
				                     []( RandomIterator f, RandomIterator l, LessCompare cmp ) { std::stable_sort( f, l, cmp ); },
				                     std::move( compare ), std::move( ts ) );
			}

			template<typename T, typename RandomIterator, typename BinaryOp>
			auto reduce( RandomIterator first, RandomIterator last, T init, BinaryOp binary_op,
			             task_scheduler ts = get_task_scheduler( ) ) {
				return impl::parallel_reduce( first, last, std::move( init ), binary_op, std::move( ts ) );
			}

			template<typename T, typename RandomIterator>
			auto reduce( RandomIterator first, RandomIterator last, T init, task_scheduler ts = get_task_scheduler( ) ) {
				using value_type = typename std::iterator_traits<RandomIterator>::value_type;
				return ::daw::algorithm::parallel::reduce(
				    first, last, std::move( init ),
				    []( auto const &lhs, auto const &rhs ) -> value_type { return lhs + rhs; }, std::move( ts ) );
			}

			template<typename RandomIterator>
			auto reduce( RandomIterator first, RandomIterator last, task_scheduler ts = get_task_scheduler( ) ) {
				using value_type = typename std::iterator_traits<RandomIterator>::value_type;
				return ::daw::algorithm::parallel::reduce( first, last, value_type{}, std::move( ts ) );
			}

			template<typename RandomIterator,
			         typename LessCompare = std::less<std::decay_t<decltype( *std::declval<RandomIterator>( ) )>>>
			auto min_element( RandomIterator first, RandomIterator last, task_scheduler ts = get_task_scheduler( ),
			                  LessCompare compare = LessCompare{} ) {

				using binary_pred_result_t = daw::is_detected<details::binary_op_arg_test, RandomIterator, LessCompare>;
				static_assert(
				    binary_pred_result_t::value,
				    "LessCompare passed to min_element must take two values referenced by first. e.g compare( "
				    "*first, *(first+1) ) "
				    "must be valid" );
				static_assert( std::is_convertible<typename binary_pred_result_t::type, bool>::value,
				               "Result of LessCompare compare must be convertible to bool" );

				return impl::parallel_min_element( first, last, compare, std::move( ts ) );
			}

			template<typename RandomIterator,
			         typename LessCompare = std::less<std::decay_t<decltype( *std::declval<RandomIterator>( ) )>>>
			auto max_element( RandomIterator first, RandomIterator const last, task_scheduler ts = get_task_scheduler( ),
			                  LessCompare compare = LessCompare{} ) {

				using binary_pred_result_t = daw::is_detected<details::binary_op_arg_test, RandomIterator, LessCompare>;
				static_assert(
				    binary_pred_result_t::value,
				    "LessCompare passed to max_element must take two values referenced by first. e.g compare( "
				    "*first, *(first+1) ) "
				    "must be valid" );
				static_assert( std::is_convertible<typename binary_pred_result_t::type, bool>::value,
				               "Result of LessCompare compare must be convertible to bool" );

				return impl::parallel_max_element( first, last, compare, std::move( ts ) );
			}

			template<typename RandomIterator, typename RandomOutputIterator, typename UnaryOperation>
			void transform( RandomIterator first1, RandomIterator const last1, RandomOutputIterator first2, UnaryOperation unary_op,
			                task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::is_detected_v<details::unary_op_arg_test, RandomIterator, UnaryOperation>,
				               "UnaryOperation passed to transform must accept the value referenced by first. e.g unary_op( *first ) "
				               "must be valid" );
				impl::parallel_map( first1, last1, first2, unary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename UnaryOperation>
			void transform( RandomIterator first, RandomIterator last, UnaryOperation unary_op,
			                task_scheduler ts = get_task_scheduler( ) ) {

				static_assert( daw::is_detected_v<details::unary_op_arg_test, RandomIterator, UnaryOperation>,
				               "UnaryOperation passed to transform must accept the value referenced by first. e.g unary_op( *first ) "
				               "must be valid" );
				impl::parallel_map( first, last, first, unary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename UnaryOperation, typename BinaryOperation>
			auto map_reduce( RandomIterator first, RandomIterator last, UnaryOperation map_function, BinaryOperation reduce_function,
			                 task_scheduler ts = get_task_scheduler( ) ) {

				using unary_op_t = daw::is_detected<details::unary_op_arg_test, RandomIterator, UnaryOperation>;
				static_assert( unary_op_t::value,
				               "UnaryOperation map_function passed to map_reduce must accept the value referenced by "
				               "first. e.g map_function( *first ) "
				               "must be valid" );

				static_assert( daw::is_detected_v<details::reduce_arg_test, typename unary_op_t::type, BinaryOperation>,
				               "BinaryOperation reduce_function passed to map_reduce must accept two args of a type "
				               "convertable from the redult of map_function. e.g. reduce_function( map_functon( *first "
				               "), map_function( *(first + 1) ) ) must be valid." );

				auto it_init = first;
				std::advance( first, 1 );
				return impl::parallel_map_reduce( first, last, *it_init, map_function, reduce_function,
				                                  std::move( ts ) );
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

				using unary_op_t = daw::is_detected<details::unary_op_arg_test, RandomIterator, UnaryOperation>;
				static_assert( unary_op_t::value,
				               "UnaryOperation map_function passed to map_reduce must accept the value referenced by "
				               "first. e.g map_function( *first ) "
				               "must be valid" );

				static_assert( daw::is_detected_v<details::reduce_arg_test, typename unary_op_t::type, BinaryOperation>,
				               "BinaryOperation reduce_function passed to map_reduce must accept two args of a type "
				               "convertable from the redult of map_function. e.g. reduce_function( map_functon( *first "
				               "), map_function( *(first + 1) ) ) must be valid." );

				auto it_init = first;
				std::advance( first, 1 );
				return impl::parallel_map_reduce( first, last, *it_init, map_function, reduce_function,
				                                  std::move( ts ) );
			}

			template<typename RandomIterator, typename RandomOutputIterator, typename BinaryOperation>
			void scan( RandomIterator first, RandomIterator last, RandomOutputIterator first_out, RandomOutputIterator last_out,
			           BinaryOperation binary_op, task_scheduler ts = get_task_scheduler( ) ) {

				using binary_op_result_t = daw::is_detected<details::binary_op_arg_test, RandomIterator, BinaryOperation>;
				static_assert(
				    binary_op_result_t::value,
				    "BinaryOperation passed to scan must take two values referenced by first. e.g binary_op( "
				    "*first, *(first+1) ) "
				    "must be valid" );

				static_assert( daw::is_detected_v<details::is_assignable_test, decltype( *first_out ),
				                                  typename binary_op_result_t::type>,
				               "The result of the binary_op must be assignable to the referenced range values. e.g. "
				               "*first_out = binary_op( *first, *(first+1) )" );


				impl::parallel_scan( first, last, first_out, last_out, binary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename RandomOutputIterator, typename BinaryOperation>
			void scan( RandomIterator first, RandomIterator last, BinaryOperation binary_op, task_scheduler ts = get_task_scheduler( ) ) {

				using binary_op_result_t = daw::is_detected<details::binary_op_arg_test, RandomIterator, BinaryOperation>;
				static_assert(
				    binary_op_result_t::value,
				    "BinaryOperation passed to scan must take two values referenced by first. e.g binary_op( "
				    "*first, *(first+1) ) "
				    "must be valid" );
				static_assert( daw::is_detected_v<details::is_assignable_test, decltype( *first ),
				                                  typename binary_op_result_t::type>,
				               "The result of the binary_op must be assignable to the referenced range values. e.g. "
				               "*first = binary_op( *first, *(first+1) )" );

				impl::parallel_scan( first, last, first, last, binary_op, std::move( ts ) );
			}

			template<typename RandomIterator, typename UnaryPredicate>
			RandomIterator find_if( RandomIterator first, RandomIterator last, UnaryPredicate pred,
			                  task_scheduler ts = get_task_scheduler( ) ) {
				return impl::parallel_find_if( first, last, pred, std::move( ts ) );
			}

			template<typename RandomIterator1, typename RandomIterator2, typename BinaryPredicate>
			bool equal( RandomIterator1 first1, RandomIterator1 last1, RandomIterator2 first2, RandomIterator2 last2, BinaryPredicate pred,
			            task_scheduler ts = get_task_scheduler( ) ) {

				return impl::parallel_equal( first1, last1, first2, last2, pred, std::move( ts ) );
			}

			template<typename RandomIterator1, typename RandomIterator2, typename BinaryPredicate>
			bool equal( RandomIterator1 first1, RandomIterator1 last1, RandomIterator2 first2, RandomIterator2 last2,
			            task_scheduler ts = get_task_scheduler( ) ) {

				auto const pred = []( auto const &lhs, auto const &rhs ) { return lhs == rhs; };
				return impl::parallel_equal( first1, last1, first2, last2, pred, std::move( ts ) );
			}

		} // namespace parallel
	}     // namespace algorithm
} // namespace daw

