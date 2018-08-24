// The MIT License (MIT)
//
// Copyright (c) 2016-2018 Darrell Wright
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

#include <memory>
#include <tuple>
#include <utility>

#include <daw/daw_value_ptr.h>

namespace daw {
	template<typename Result, typename Functions, typename... Args>
	struct package_t;

	template<typename Result, typename Functions, typename... Args>
	std::shared_ptr<package_t<Result, Functions, Args...>>
	make_shared_package( bool continue_on_result_destruction, Result &&result,
	                     Functions &&functions, Args &&... args );

	template<typename R, template<typename> class WeakPtr>
	constexpr R *weak_ptr_test( WeakPtr<R> ) {
		return static_cast<R *>( nullptr );
	}

	template<typename T>
	struct weak_ptr_type_impl {
		using type = decltype( *weak_ptr_test( std::declval<T>( ) ) );
	};

	template<typename T>
	using weak_ptr_type_t = typename weak_ptr_type_impl<T>::type;

	namespace impl {
		template<typename functions_t, typename arguments_t, typename result_t, typename... Args>
		struct members_t {
			functions_t m_function_list;
			arguments_t m_targs;
			result_t m_result;
			bool m_continue_on_result_destruction;

			constexpr members_t( bool continueonclientdestruction, result_t result,
			                     functions_t functions, Args... args )
			  : m_function_list( std::move( functions ) )
			  , m_targs( std::make_tuple( std::move( args )... ) )
			  , m_result( result )
			  , m_continue_on_result_destruction( continueonclientdestruction ) {}
		}; // members_t
	} // namespace impl

	template<typename Result, typename Functions, typename... Args>
	struct package_t {
		using functions_t = Functions;
		using arguments_t = std::tuple<std::decay_t<Args>...>;
		using result_t = Result;
		using result_value_t = weak_ptr_type_t<result_t>;

	private:
		daw::value_ptr<impl::members_t<functions_t, arguments_t, result_t, Args...>> members;

	public:
		package_t( ) = delete;
		package_t( package_t const & ) = delete;
		package_t &operator=( package_t const & ) = delete;

		~package_t( ) noexcept = default;

		constexpr package_t( package_t && ) noexcept = default;
		constexpr package_t &operator=( package_t && ) noexcept = default;

		constexpr package_t( bool continueonclientdestruction, result_t result,
		           functions_t functions, Args &&... args )
		  : members( continueonclientdestruction, std::move( result ),
		             std::move( functions ), std::forward<Args>( args )... ) {}

		constexpr functions_t const &function_list( ) const noexcept {
			return members->m_function_list;
		}

		constexpr functions_t &function_list( ) noexcept {
			return members->m_function_list;
		}

		constexpr result_t const &result( ) const noexcept {
			return members->m_result;
		}

		constexpr result_t &result( ) noexcept {
			return members->m_result;
		}

		constexpr bool continue_processing( ) const {
			return !destination_expired( ) || continue_on_result_destruction( );
		}

		template<typename... NewArgs>
		decltype( auto ) next_package( NewArgs &&... nargs ) {
			return make_shared_package( continue_on_result_destruction( ),
			                            std::move( members->m_result ),
			                            std::move( members->m_function_list ),
			                            std::forward<NewArgs>( nargs )... );
		}

		constexpr bool destination_expired( ) const {
			return result( ).expired( );
		}

		constexpr arguments_t const &targs( ) const noexcept {
			return members->m_targs;
		}

		constexpr arguments_t &targs( ) noexcept {
			return members->m_targs;
		}

	private:
		constexpr bool const &continue_on_result_destruction( ) const noexcept {
			return members->m_continue_on_result_destruction;
		}

		constexpr bool &continue_on_result_destruction( ) noexcept {
			return members->m_continue_on_result_destruction;
		}
	}; // package_t

	template<typename Result, typename Functions, typename... Args>
	package_t<Result, Functions, Args...>
	make_package( bool continue_on_result_destruction, Result &&result,
	              Functions &&functions, Args &&... args ) {
		return package_t<Result, Functions, Args...>(
		  continue_on_result_destruction, std::forward<Result>( result ),
		  std::forward<Functions>( functions ), std::forward<Args>( args )... );
	}

	template<typename Result, typename Functions, typename... Args>
	std::shared_ptr<package_t<Result, Functions, Args...>>
	make_shared_package( bool continue_on_result_destruction, Result &&result,
	                     Functions &&functions, Args &&... args ) {
		return std::make_shared<package_t<Result, Functions, Args...>>(
		  make_package(
		    continue_on_result_destruction, std::forward<Result>( result ),
		    std::forward<Functions>( functions ), std::forward<Args>( args )... ) );
	}

} // namespace daw
