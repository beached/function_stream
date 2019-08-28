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

#include <daw/daw_sort_n.h>
#include <daw/daw_view.h>

#include "impl/algorithms_impl.h"
#include "impl/concept_checks.h"

namespace daw::algorithm::parallel {
	template<typename RandomIterator, typename UnaryOperation>
	void for_each( RandomIterator first, RandomIterator last,
	               UnaryOperation unary_op,
	               task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		static_assert(
		  concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
		  "UnaryOperation passed to for_each must accept the value referenced "
		  "by first. e.g "
		  "unary_op( *first ) must be valid" );

		impl::parallel_for_each( daw::view( first, last ),
		                         ::daw::traits::lift_func( unary_op ),
		                         daw::move( ts ) );
	}

	template<typename RandomIterator, typename UnaryOperation>
	void for_each_n( RandomIterator first, size_t N, UnaryOperation unary_op,
	                 task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		static_assert(
		  concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
		  "UnaryOperation passed to for_each_n must accept the value "
		  "referenced by first. e.g "
		  "unary_op( *first ) must be valid" );

		auto const last = ::std::next( first, static_cast<intmax_t>( N ) );
		impl::parallel_for_each( daw__view( first, last ),
		                         ::daw::traits::lift_func( unary_op ),
		                         daw::move( ts ) );
	}

	template<typename RandomIterator, typename UnaryOperation>
	void for_each_index( RandomIterator first, RandomIterator last,
	                     UnaryOperation indexed_op,
	                     task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		static_assert( traits::is_callable_v<UnaryOperation, size_t>,
		               "UnaryOperation passed to "
		               "for_each_index must a size_t argument "
		               "unary_op( (size_t)5 ) must be valid" );

		impl::parallel_for_each_index( daw::view( first, last ),
		                               ::daw::traits::lift_func( indexed_op ),
		                               daw::move( ts ) );
	}

	template<typename RandomIterator, typename T>
	void fill( RandomIterator first, RandomIterator last, T const &value,
	           task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		static_assert( traits::is_assignable_iterator_v<RandomIterator, T>,
		               "T value must be assignable to the "
		               "dereferenced RandomIterator first. "
		               "e.g. *first = value is valid" );
		impl::parallel_for_each(
		  daw::view( first, last ), [&value]( auto &item ) { item = value; },
		  daw::move( ts ) );
	}

	template<typename RandomIterator, typename Compare = ::std::less<>>
	void sort( RandomIterator first, RandomIterator last,
	           task_scheduler ts = get_task_scheduler( ),
	           Compare &&comp = Compare{} ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		concept_checks::is_binary_predicate_test<Compare, RandomIterator,
		                                         RandomIterator>( );
		impl::parallel_sort(
		  daw::view( first, last ),
		  []( RandomIterator f, RandomIterator l, Compare cmp ) {
			  ::std::sort( f, l, cmp );
		  },
		  ::daw::traits::lift_func( ::std::forward<Compare>( comp ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator, typename Compare = ::std::less<>>
	void stable_sort( RandomIterator first, RandomIterator last,
	                  task_scheduler ts = get_task_scheduler( ),
	                  Compare &&comp = Compare{} ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		concept_checks::is_binary_predicate_test<Compare, RandomIterator,
		                                         RandomIterator>( );

		impl::parallel_sort(
		  daw::view( first, last ),
		  []( RandomIterator f, RandomIterator l, Compare cmp ) {
			  ::std::stable_sort( f, l, cmp );
		  },
		  ::daw::traits::lift_func( ::std::forward<Compare>( comp ) ),
		  daw::move( ts ) );
	}

	template<typename T, typename RandomIterator, typename BinaryOperation>
	[[nodiscard]] T reduce( RandomIterator first, RandomIterator last, T init,
	                        BinaryOperation &&binary_op,
	                        task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		static_assert(
		  concept_checks::is_callable_v<BinaryOperation, RandomIterator,
		                                RandomIterator>,
		  "BinaryOperation passed to reduce must take two values referenced by "
		  "first. e.g "
		  "binary_op( "
		  "*first, *(first+1) ) "
		  "must be valid" );

		static_assert(
		  ::std::is_convertible<
		    concept_checks::is_callable_t<BinaryOperation, RandomIterator,
		                                  RandomIterator>,
		    typename ::std::iterator_traits<RandomIterator>::value_type>::value,
		  "Result of BinaryOperation must be convertable to type of value "
		  "referenced by "
		  "RandomIterator. "
		  "e.g. *first = binary_op( *first, *(first + 1) ) must be valid." );

		return impl::parallel_reduce(
		  daw::view( first, last ), daw::move( init ),
		  ::daw::traits::lift_func( ::std::forward<BinaryOperation>( binary_op ) ),
		  daw::move( ts ) );
	}

	template<typename T, typename RandomIterator>
	[[nodiscard]] T reduce( RandomIterator first, RandomIterator last, T init,
	                        task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		return ::daw::algorithm::parallel::reduce(
		  first, last, daw::move( init ), ::std::plus<>{}, daw::move( ts ) );
	}

	template<typename RandomIterator>
	[[nodiscard]] decltype( auto )
	reduce( RandomIterator first, RandomIterator last,
	        task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		using value_type =
		  typename ::std::iterator_traits<RandomIterator>::value_type;
		return ::daw::algorithm::parallel::reduce( daw::view( first, last ),
		                                           value_type{}, daw::move( ts ) );
	}

	template<typename RandomIterator, typename Compare = ::std::less<>>
	[[nodiscard]] decltype( auto )
	min_element( RandomIterator first, RandomIterator last,
	             task_scheduler ts = get_task_scheduler( ),
	             Compare &&comp = Compare{} ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		traits::is_input_iterator_test<RandomIterator>( );
		concept_checks::is_binary_predicate_test<Compare, RandomIterator,
		                                         RandomIterator>( );

		return impl::parallel_min_element(
		  daw::view( first, last ),
		  ::daw::traits::lift_func( ::std::forward<Compare>( comp ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator, typename Compare = ::std::less<>>
	[[nodiscard]] decltype( auto )
	max_element( RandomIterator first, RandomIterator const last,
	             task_scheduler ts = get_task_scheduler( ),
	             Compare comp = Compare{} ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		concept_checks::is_binary_predicate_test<Compare, RandomIterator,
		                                         RandomIterator>( );

		return impl::parallel_max_element(
		  daw::view( first, last ),
		  ::daw::traits::lift_func( ::std::forward<Compare>( comp ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator, typename RandomOutputIterator,
	         typename UnaryOperation>
	void transform( RandomIterator first, RandomIterator const last,
	                RandomOutputIterator first_out, UnaryOperation &&unary_op,
	                task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		static_assert(
		  concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
		  "UnaryOperation passed to transform must accept the value referenced "
		  "by first. e.g "
		  "unary_op( *first ) must be valid" );
		static_assert(
		  traits::is_assignable_iterator_v<
		    RandomOutputIterator,
		    concept_checks::is_callable_t<UnaryOperation, RandomIterator>>,
		  "The result of the UnaryOperation must be assignable to the "
		  "dereferenced "
		  "RandomOutputIterator. e.g. *first_out = unary_op( *first ) must be "
		  "valid" );

		impl::parallel_map(
		  daw::view( first, last ), first_out,
		  ::daw::traits::lift_func( ::std::forward<UnaryOperation>( unary_op ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator1, typename RandomIterator2,
	         typename RandomOutputIterator, typename BinaryOperation>
	void transform( RandomIterator1 first1, RandomIterator1 const last1,
	                RandomIterator2 first2, RandomOutputIterator first_out,
	                BinaryOperation &&binary_op,
	                task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator1>( );
		traits::is_random_access_iterator_test<RandomIterator2>( );
		static_assert(
		  concept_checks::is_callable_v<BinaryOperation, RandomIterator1,
		                                RandomIterator2>,
		  "BinaryOperation passed to transform must accept the value "
		  "referenced "
		  "by first1 and first2. e.g "
		  "unary_op( *first1, *first2 ) must be valid" );
		static_assert(
		  traits::is_assignable_iterator_v<
		    RandomOutputIterator,
		    concept_checks::is_callable_t<BinaryOperation, RandomIterator1,
		                                  RandomIterator2>>,
		  "The result of the BinaryOperation must be assignable to the "
		  "dereferenced "
		  "RandomOutputIterator. e.g. *first_out = binary_op( *first1, *first2 "
		  ") must be "
		  "valid" );

		impl::parallel_map(
		  daw::view( first1, last1 ), first2, first_out,
		  ::daw::traits::lift_func( ::std::forward<BinaryOperation>( binary_op ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator, typename UnaryOperation>
	void transform( RandomIterator first, RandomIterator last,
	                UnaryOperation &&unary_op,
	                task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		static_assert(
		  concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
		  "UnaryOperation passed to transform must accept the value referenced "
		  "by first. e.g "
		  "unary_op( *first ) must be valid" );
		static_assert(
		  traits::is_assignable_iterator_v<
		    RandomIterator,
		    concept_checks::is_callable_t<UnaryOperation, RandomIterator>>,
		  "The result of the UnaryOperation must be assignable to the "
		  "dereferenced "
		  "RandomIterator. e.g. *first_out = unary_op( *first ) must be "
		  "valid" );

		impl::parallel_map(
		  daw::view( first, last ), first,
		  ::daw::traits::lift_func( ::std::forward<UnaryOperation>( unary_op ) ),
		  daw::move( ts ) );
	}

	template<
	  typename RandomIterator, typename UnaryOperation, typename BinaryOperation,
	  ::std::enable_if_t<traits::is_random_access_iterator_v<RandomIterator>,
	                     ::std::nullptr_t> = nullptr>
	[[nodiscard]] decltype( auto )
	map_reduce( RandomIterator first, RandomIterator last,
	            UnaryOperation &&map_function, BinaryOperation &&reduce_function,
	            ::daw::task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		static_assert(
		  concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
		  "UnaryOperation map_function passed to map_reduce must accept the "
		  "value "
		  "referenced by first. e.g "
		  "map_function( *first ) must be valid" );

		using transform_result_t =
		  concept_checks::is_callable_t<UnaryOperation, RandomIterator>;

		static_assert( traits::is_callable_v<BinaryOperation, transform_result_t,
		                                     transform_result_t>,
		               "BinaryOperation reduce_function passed to map_reduce "
		               "must accept the result of the "
		               "transform_function for each arg. e.g "
		               "reduce_function( tranfom_function( *first ), "
		               "transform_function( *(first + 1) ) ) must "
		               "be valid" );

		auto it_init = first;
		std::advance( first, 1 );
		return impl::parallel_map_reduce(
		  daw::view( first, last ), *it_init,
		  ::daw::traits::lift_func(
		    ::std::forward<UnaryOperation>( map_function ) ),
		  ::daw::traits::lift_func(
		    ::std::forward<BinaryOperation>( reduce_function ) ),
		  daw::move( ts ) );
	}

	/// @brief Perform MapReduce on range and return result
	/// @tparam RandomIterator Type of Range RandomIterators
	/// @tparam T Type of initial value
	/// @tparam UnaryOperation Function that maps a->a'
	/// @tparam BinaryOperation Function that takes to items in range and
	/// returns 1
	/// @param first Beginning of range
	/// @param last End of range
	/// @param init initial value to supply map/reduce
	/// @param map_function unary function that maps source value to argument
	/// of reduce_function
	/// @param reduce_function binary function that maps results of
	/// map_function to resulting value
	/// @return Value from reduce function after range is of size 1
	template<typename RandomIterator, typename T, typename UnaryOperation,
	         typename BinaryOperation,
	         ::std::enable_if_t<
	           !::std::is_same_v<::daw::task_scheduler,
	                             ::daw::remove_cvref_t<BinaryOperation>>,
	           ::std::nullptr_t> = nullptr>
	[[nodiscard]] decltype( auto )
	map_reduce( RandomIterator first, RandomIterator last, T const &init,
	            UnaryOperation &&map_function, BinaryOperation &&reduce_function,
	            ::daw::task_scheduler ts = get_task_scheduler( ) ) {
		/*
		        traits::is_random_access_iterator_test<RandomIterator>( );
		        static_assert(
		          concept_checks::is_callable_v<UnaryOperation, RandomIterator>,
		          "UnaryOperation map_function passed to map_reduce must accept
		   the " "value " "referenced by first. e.g " "map_function( *first )
		   must be valid" );

		        using transform_result_t =
		          concept_checks::is_callable_t<UnaryOperation, RandomIterator>;
		        static_assert(
		          concept_checks::is_callable_v<BinaryOperation,
		   transform_result_t, transform_result_t>, "BinaryOperation
		   reduce_function passed to map_reduce must accept " "the result of the
		   " "transform_function for each arg. e.g " "reduce_function(
		   tranfom_function( *first ), transform_function( "
		          "*(first + 1) ) ) must "
		          "be valid" );
		*/
		return impl::parallel_map_reduce(
		  daw::view( first, last ), init,
		  ::daw::traits::lift_func(
		    ::std::forward<UnaryOperation>( map_function ) ),
		  ::daw::traits::lift_func(
		    ::std::forward<BinaryOperation>( reduce_function ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator, typename RandomOutputIterator,
	         typename BinaryOperation>
	void scan( RandomIterator first, RandomIterator last,
	           RandomOutputIterator first_out, RandomOutputIterator last_out,
	           BinaryOperation &&binary_op,
	           task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		traits::is_input_iterator_test<RandomIterator>( );
		traits::is_random_access_iterator_test<RandomOutputIterator>( );
		static_assert(
		  concept_checks::is_callable_v<BinaryOperation, RandomIterator,
		                                RandomIterator>,
		  "BinaryOperation passed to scan must take two values referenced by "
		  "first. e.g "
		  "binary_op( *first, *(first+1) ) must be valid" );

		traits::is_output_iterator_test<
		  RandomOutputIterator,
		  concept_checks::is_callable_t<BinaryOperation, RandomIterator,
		                                RandomIterator>>( );

		impl::parallel_scan(
		  daw::view( first, last ), daw::view( first_out, last_out ),
		  ::daw::traits::lift_func( ::std::forward<BinaryOperation>( binary_op ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator, typename BinaryOperation>
	void scan( RandomIterator first, RandomIterator last,
	           BinaryOperation &&binary_op,
	           task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		static_assert(
		  concept_checks::is_callable_v<BinaryOperation, RandomIterator,
		                                RandomIterator>,
		  "BinaryOperation passed to scan must take two values referenced by "
		  "first. e.g "
		  "binary_op( *first, *(first+1) ) must be valid" );

		// Ensure that the result of binary operation can be assigned to the
		// iterator
		traits::is_output_iterator_test<
		  RandomIterator, concept_checks::is_callable_t<
		                    BinaryOperation, RandomIterator, RandomIterator>>( );

		impl::parallel_scan(
		  daw::view( first, last ), first, last,
		  ::daw::traits::lift_func( ::std::forward<BinaryOperation>( binary_op ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator, typename UnaryPredicate>
	[[nodiscard]] RandomIterator
	find_if( RandomIterator first, RandomIterator last, UnaryPredicate &&pred,
	         task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		concept_checks::is_unary_predicate_test<UnaryPredicate, RandomIterator>( );

		return impl::parallel_find_if(
		  daw::view( first, last ),
		  ::daw::traits::lift_func( ::std::forward<UnaryPredicate>( pred ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator1, typename RandomIterator2,
	         typename BinaryPredicate>
	[[nodiscard]] bool equal( RandomIterator1 first1, RandomIterator1 last1,
	                          RandomIterator2 first2, RandomIterator2 last2,
	                          BinaryPredicate &&pred,
	                          task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator1>( );
		traits::is_input_iterator_test<RandomIterator1>( );
		traits::is_random_access_iterator_test<RandomIterator2>( );
		traits::is_input_iterator_test<RandomIterator2>( );
		concept_checks::is_binary_predicate_test<BinaryPredicate, RandomIterator1,
		                                         RandomIterator2>( );

		return impl::parallel_equal(
		  first1, last1, first2, last2,
		  ::daw::traits::lift_func( ::std::forward<BinaryPredicate>( pred ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator1, typename RandomIterator2>
	[[nodiscard]] bool equal( RandomIterator1 first1, RandomIterator1 last1,
	                          RandomIterator2 first2, RandomIterator2 last2,
	                          task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator1>( );
		traits::is_input_iterator_test<RandomIterator1>( );
		traits::is_random_access_iterator_test<RandomIterator2>( );
		traits::is_input_iterator_test<RandomIterator2>( );
		concept_checks::is_equality_comparable_test<RandomIterator1,
		                                            RandomIterator2>( );

		auto pred = []( auto const &lhs, auto const &rhs ) { return lhs == rhs; };
		return impl::parallel_equal( first1, last1, first2, last2,
		                             ::daw::move( pred ), daw::move( ts ) );
	}

	template<typename RandomIterator, typename UnaryPredicate>
	[[nodiscard]] decltype( auto )
	count_if( RandomIterator first, RandomIterator last, UnaryPredicate &&pred,
	          task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		concept_checks::is_unary_predicate_test<UnaryPredicate, RandomIterator>( );

		return impl::parallel_count(
		  daw::view( first, last ),
		  ::daw::traits::lift_func( ::std::forward<UnaryPredicate>( pred ) ),
		  daw::move( ts ) );
	}

	template<typename RandomIterator, typename T>
	[[nodiscard]] decltype( auto )
	count( RandomIterator first, RandomIterator last, T const &value,
	       task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		traits::is_input_iterator_test<RandomIterator>( );

		return impl::parallel_count(
		  first, last, [&value]( auto const &rhs ) { return value == rhs; },
		  daw::move( ts ) );
	}

	template<size_t minimum_size = 1>
	using default_range_splitter = impl::split_range_t<minimum_size>;

	template<typename PartitionPolicy = default_range_splitter<>,
	         typename RandomIterator, typename Function>
	void chunked_for_each( RandomIterator first, RandomIterator last,
	                       Function &&func,
	                       task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		auto ranges = PartitionPolicy{}( daw::view( first, last ), ts.size( ) );
		impl::partition_range(
		  ranges, ::daw::traits::lift_func( ::std::forward<Function>( func ) ),
		  daw::move( ts ) )
		  .wait( );
	}

	template<typename PartitionPolicy = default_range_splitter<>,
	         typename RandomIterator, typename Function>
	void chunked_for_each_pos( RandomIterator first, RandomIterator last,
	                           Function &&func,
	                           task_scheduler ts = get_task_scheduler( ) ) {

		traits::is_random_access_iterator_test<RandomIterator>( );
		auto ranges = PartitionPolicy{}( daw::view( first, last ), ts.size( ) );
		impl::partition_range_pos(
		  ranges, ::daw::traits::lift_func( ::std::forward<Function>( func ) ),
		  ::daw::move( ts ) )
		  .wait( );
	}
} // namespace daw::algorithm::parallel
