// The MIT License (MIT)
//
// Copyright (c) 2017-2019 Darrell Wright
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

#include <iterator>

#include <daw/daw_traits.h>

namespace daw::concept_checks {
	template<typename BinaryPredicate, typename Iterator1, typename Iterator2>
	inline constexpr bool is_binary_predicate_v =
	  traits::is_binary_predicate<BinaryPredicate,
	                              typename std::iterator_traits<Iterator1>::value_type,
	                              typename std::iterator_traits<Iterator2>::value_type>;

	template<typename BinaryPredicate, typename Iterator1, typename Iterator2>
	constexpr bool is_binary_predicate_test( ) noexcept {
		static_assert( is_binary_predicate_v<BinaryPredicate, Iterator1, Iterator2>,
		               "Supplied BinaryPredicate does not satisfy the concept of "
		               "BinaryPredicate.  See "
		               "http://en.cppreference.com/w/cpp/concept/BinaryPredicate" );
		return true;
	}

	template<typename UnaryPredicate, typename Iterator1>
	inline constexpr bool is_unary_predicate_v =
	  traits::is_unary_predicate_v<UnaryPredicate,
	                               typename std::iterator_traits<Iterator1>::value_type>;

	template<typename UnaryPredicate, typename Iterator1>
	constexpr bool is_unary_predicate_test( ) noexcept {
		static_assert( is_unary_predicate_v<UnaryPredicate, Iterator1>,
		               "Supplied UnaryPredicate does not satisfy the concept of "
		               "UnaryPredicate.  See "
		               "http://en.cppreference.com/w/cpp/concept/Predicate" );
		return true;
	}

	template<typename Iterator1, typename Iterator2>
	inline constexpr bool is_equality_comparable_v =
	  traits::is_equality_comparable_v<typename std::iterator_traits<Iterator1>::value_type,
	                                   typename std::iterator_traits<Iterator2>::value_type>;

	template<typename Iterator1, typename Iterator2>
	constexpr bool is_equality_comparable_test( ) noexcept {
		static_assert( is_equality_comparable_v<Iterator1, Iterator2>,
		               "Dereferenced Iterator1 and Iterator2 must be equality "
		               "comparable e.g. "
		               "*first1 == *first2 must be valid" );
		return true;
	}

	template<typename T>
	using value_of_deref_t = daw::remove_cvref_t<decltype( *std::declval<T>( ) )>;

	template<typename Operator, typename... Iterators>
	inline constexpr bool const is_callable_v =
	  std::is_invocable_v<Operator, value_of_deref_t<Iterators>...>;

	template<typename Operator, typename... Iterators>
	using is_callable_t = traits::is_callable_t<Operator, value_of_deref_t<Iterators>...>;

	template<typename T>
	concept Function = std::is_function_v<T>;
} // namespace daw::concept_checks
