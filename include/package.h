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

#include <memory>
#include <tuple>
#include <utility>

#include "future_result.h"
#include "task_scheduler.h"

#include <daw/cpp_17.h>

namespace daw {
	template<typename Result, typename Functions, typename... Args>
	auto make_shared_package( bool, Result &&, Functions &&, Args &&... );

	template<typename R>
	constexpr auto weak_ptr_test( std::weak_ptr<R> wp ) {
		return static_cast<R *>( nullptr );
	}

	template<typename T>
	struct weak_ptr_type_impl {
		using type = decltype( *weak_ptr_test( std::declval<T>( ) ) );
	};

	template<typename T>
	using weak_ptr_type_t = typename weak_ptr_type_impl<T>::type;

	template<typename Result, typename Functions, typename... Args>
	struct package_t {
		using functions_t = Functions;
		using arguments_t = std::tuple<std::decay_t<Args>...>;
		using result_t = Result;
		using result_value_t = weak_ptr_type_t<result_t>;

		functions_t f_list;
		arguments_t targs;
		result_t m_result;
		bool continue_on_result_destruction;

		package_t( ) = delete;
		~package_t( ) = default;
		package_t( package_t const & ) = delete;
		package_t( package_t && ) noexcept = default;
		package_t &operator=( package_t const & ) = delete;
		package_t &operator=( package_t && ) noexcept = default;

		bool destination_expired( ) const {
			return m_result.expired( );
		}

		bool continue_processing( ) const {
			return !destination_expired( ) || continue_on_result_destruction;
		}

		template<typename... NewArgs>
		auto next_package( NewArgs... nargs ) {
			return make_shared_package( continue_on_result_destruction, std::move( m_result ), std::move( f_list ),
			                            std::move( nargs )... );
		}

		package_t( bool continueonclientdestruction, result_t result, functions_t functions, Args... args )
		    : f_list{std::move( functions )}
		    , targs{std::make_tuple( std::move( args )... )}
		    , m_result{result}
		    , continue_on_result_destruction{continueonclientdestruction} {}
	}; // package_t

	template<typename Result, typename Functions, typename... Args>
	auto make_package( bool continue_on_result_destruction, Result &&result, Functions &&functions, Args &&... args ) {
		return package_t<Result, Functions, Args...>{continue_on_result_destruction, std::forward<Result>( result ),
		                                             std::forward<Functions>( functions ),
		                                             std::forward<Args>( args )...};
	}

	template<typename Result, typename Functions, typename... Args>
	auto make_shared_package( bool continue_on_result_destruction, Result &&result, Functions &&functions,
	                          Args &&... args ) {
		return std::make_shared<package_t<Result, Functions, Args...>>(
		    make_package( continue_on_result_destruction, std::forward<Result>( result ),
		                  std::forward<Functions>( functions ), std::forward<Args>( args )... ) );
	}

} // namespace daw
