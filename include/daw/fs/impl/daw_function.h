// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/header_libraries
//

#pragma once

#include "concept_checks.h"

#include <daw/daw_concepts.h>
#include <daw/daw_move.h>

#include <ciso646>
#include <memory>
#include <tuple>
#include <type_traits>

namespace daw::fs::impl {
	template<typename T>
	struct function_info : function_info<decltype( &T::operator( ) )> {};
	// For generic types, directly use the result of the signature of its
	// 'operator()'

	template<typename ClassType, typename ReturnType, typename... Args>
	struct function_info<ReturnType ( ClassType::* )( Args... )> {
		static constexpr size_t const arity = sizeof...( Args );

		using result_type = ReturnType;
		using args_tuple = std::tuple<Args...>;
		using decayed_args_tuple = std::tuple<std::decay_t<Args>...>;

		template<size_t i>
		struct arg {
			typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
			// the i-th argument is equivalent to the i-th tuple element of a tuple
			// composed of those arguments.
		};
	};

	template<typename ClassType, typename ReturnType, typename... Args>
	struct function_info<ReturnType ( ClassType::* )( Args... ) const> {
		static constexpr size_t const arity = sizeof...( Args );

		using result_type = ReturnType;
		using args_tuple = std::tuple<Args...>;
		using decayed_args_tuple = std::tuple<std::decay_t<Args>...>;

		template<size_t i>
		struct arg {
			typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
			// the i-th argument is equivalent to the i-th tuple element of a tuple
			// composed of those arguments.
		};
	};

	template<typename ClassType, typename ReturnType, typename... Args>
	struct function_info<ReturnType ( ClassType::* )( Args... ) volatile> {
		static constexpr size_t const arity = sizeof...( Args );

		using result_type = ReturnType;
		using args_tuple = std::tuple<Args...>;
		using decayed_args_tuple = std::tuple<std::decay_t<Args>...>;

		template<size_t i>
		struct arg {
			typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
			// the i-th argument is equivalent to the i-th tuple element of a tuple
			// composed of those arguments.
		};
	};

	template<typename ClassType, typename ReturnType, typename... Args>
	struct function_info<ReturnType ( ClassType::* )( Args... ) volatile const> {
		static constexpr size_t const arity = sizeof...( Args );

		using result_type = ReturnType;
		using args_tuple = std::tuple<Args...>;
		using decayed_args_tuple = std::tuple<std::decay_t<Args>...>;

		template<size_t i>
		struct arg {
			typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
			// the i-th argument is equivalent to the i-th tuple element of a tuple
			// composed of those arguments.
		};
	};

	template<typename ClassType, typename ReturnType, typename... Args>
	struct function_info<ReturnType ( ClassType::* )( Args... ) noexcept> {
		static constexpr size_t const arity = sizeof...( Args );

		using result_type = ReturnType;
		using args_tuple = std::tuple<Args...>;
		using decayed_args_tuple = std::tuple<std::decay_t<Args>...>;

		template<size_t i>
		struct arg {
			typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
			// the i-th argument is equivalent to the i-th tuple element of a tuple
			// composed of those arguments.
		};
	};

	template<typename ClassType, typename ReturnType, typename... Args>
	struct function_info<ReturnType ( ClassType::* )( Args... ) const noexcept> {
		static constexpr size_t const arity = sizeof...( Args );

		using result_type = ReturnType;
		using args_tuple = std::tuple<Args...>;
		using decayed_args_tuple = std::tuple<std::decay_t<Args>...>;

		template<size_t i>
		struct arg {
			typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
			// the i-th argument is equivalent to the i-th tuple element of a tuple
			// composed of those arguments.
		};
	};

	template<typename ClassType, typename ReturnType, typename... Args>
	struct function_info<ReturnType ( ClassType::* )( Args... ) volatile noexcept> {
		static constexpr size_t const arity = sizeof...( Args );

		using result_type = ReturnType;
		using args_tuple = std::tuple<Args...>;
		using decayed_args_tuple = std::tuple<std::decay_t<Args>...>;

		template<size_t i>
		struct arg {
			typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
			// the i-th argument is equivalent to the i-th tuple element of a tuple
			// composed of those arguments.
		};
	};

	template<typename ClassType, typename ReturnType, typename... Args>
	struct function_info<ReturnType ( ClassType::* )( Args... ) volatile const noexcept> {
		static constexpr size_t const arity = sizeof...( Args );

		using result_type = ReturnType;
		using args_tuple = std::tuple<Args...>;
		using decayed_args_tuple = std::tuple<std::decay_t<Args>...>;

		template<size_t i>
		struct arg {
			typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
			// the i-th argument is equivalent to the i-th tuple element of a tuple
			// composed of those arguments.
		};
	};

	template<typename Function>
	requires( not concept_checks::Function<std::remove_cvref_t<Function>> ) //
	  constexpr decltype( auto ) make_callable( Function &&func ) noexcept {
		return DAW_FWD( func );
	}

	namespace function_impl {
		template<typename Function>
		struct fp_callable_t {
			static_assert( std::is_function_v<Function> );
			Function *fp = nullptr;

			template<typename... Args>
			requires( not invocable_result<Function, void, Args...> ) //
			  constexpr decltype( auto )
			  operator( )( Args &&...args ) const
			  noexcept( std::is_nothrow_invocable_v<std::add_pointer_t<Function>, Args...> ) {

				return fp( DAW_FWD( args )... );
			}

			template<typename... Args>
			requires( invocable_result<Function, void, Args...> ) //
			  constexpr void
			  operator( )( Args &&...args ) const
			  noexcept( std::is_nothrow_invocable_v<std::add_pointer_t<Function>, Args...> ) {

				fp( DAW_FWD( args )... );
			}
		};
	} // namespace function_impl

	template<concept_checks::Function Function>
	constexpr auto make_callable( Function *func ) noexcept {
		return function_impl::fp_callable_t<Function>{ func };
	}
} // namespace daw::fs::impl
