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

#include <iterator>

#include <daw/daw_traits.h>

namespace daw {
	namespace concept_checks {
		template<typename BinaryPredicate, typename Iterator1, typename Iterator2>
		constexpr bool is_binary_predicate_v = daw::is_binary_predicate_v<
		  BinaryPredicate, typename std::iterator_traits<Iterator1>::value_type,
		  typename std::iterator_traits<Iterator2>::value_type>;

		template<typename UnaryPredicate, typename Iterator1>
		constexpr bool is_unary_predicate_v = daw::is_unary_predicate_v<
		  UnaryPredicate, typename std::iterator_traits<Iterator1>::value_type>;

		template<typename Iterator1, typename Iterator2>
		constexpr bool is_equality_comparable_v = daw::is_equality_comparable_v<
		  typename std::iterator_traits<Iterator1>::value_type,
		  typename std::iterator_traits<Iterator2>::value_type>;

		template<typename Operator, typename... Iterators>
		constexpr bool is_callable_v = daw::is_callable_v<
		  Operator, typename std::iterator_traits<Iterators>::value_type...>;

		template<typename Operator, typename... Iterators>
		using is_callable_t = daw::is_callable_t<
		  Operator, typename std::iterator_traits<Iterators>::value_type...>;
	} // namespace concept_checks
} // namespace daw
