// The MIT License (MIT)
//
// Copyright (c) 2016 Darrell Wright
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
#include <utility>
#include <tuple>

#include "task_scheduler.h"
#include "future_result.h"

#include <daw/cpp_17.h>

namespace daw {
	namespace impl {
		template<size_t S, typename Tuple>
		using is_function_tag = daw::bool_constant<0 <= S && S < std::tuple_size<std::decay_t<Tuple>>::value>;

		template<size_t S, typename Tuple>
		constexpr bool const is_function_tag_v = is_function_tag<S, Tuple>::value;

		template<size_t S, typename Tuple>
		using is_function_tag_t = typename is_function_tag<S, Tuple>::type;

		template<size_t pos, typename Package> struct call_task_t;
		template<typename Package> struct call_task_last_t;

		struct function_tag { using category = function_tag; };
		struct last_function_tag { using category = last_function_tag; };

		template<size_t S, typename Tuple>
		using which_type_t = typename std::conditional < S < std::tuple_size<std::decay_t<Tuple>>::value - 1, function_tag, last_function_tag>::type;

		template<size_t pos, typename Package>
		void call_task( Package, last_function_tag );
		template<size_t pos, typename Package>
		void call_task( Package, function_tag );

		template<size_t pos, typename Package>
		void call( Package package ) {
			get_task_scheduler( ).add_task( [p = std::move( package )]( ) mutable {
				call_task<pos>( std::move( p ), typename impl::which_type_t<pos, decltype(p->f_list)>::category { } );
			} );
		}

		template<size_t pos, typename Package>
		void call_task( Package package, last_function_tag ) {
			if( !package->continue_processing( ) ) {
				return;
			}
			auto func = std::get<pos>( package->f_list );
			auto client_data = package->m_result.lock( );
			if( client_data ) {
				client_data->from_code( [&]( ) mutable {
					return daw::apply( func, std::move( package->targs ) );
				} );
			} else {
				daw::apply( func, std::move( package->targs ) );
			}
		}

		template<size_t pos, typename Package>
		void call_task( Package package, function_tag ) {
			if( !package->continue_processing( ) ) {
				return;
			}
			auto func = std::get<pos>( package->f_list );
			try {
				static size_t const new_pos = pos + 1;
				call<new_pos>( package->next_package( daw::apply( func, std::move( package->targs ) ) ) );
			} catch( ... ) {
				auto result = package->m_result.lock( );
				if( result ) {
					result->from_exception( std::current_exception( ) );
				}
			}
		}

		template<typename Result, typename Functions, typename... Args>
		auto make_shared_package( bool, Result &&, Functions &&, Args&&... );

		template<typename R>
		constexpr auto weak_ptr_test( std::weak_ptr<R> wp ) { return static_cast<R*>(nullptr); }

		template<typename T>
		struct weak_ptr_type_impl {
			using type = decltype(*weak_ptr_test( std::declval<T>( ) ));
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
			package_t( package_t && ) = default;
			package_t & operator=( package_t const & ) = delete;
			package_t & operator=( package_t && ) = default;

			bool destination_expired( ) const {
				return m_result.expired( );
			}

			bool continue_processing( ) const {
				return !destination_expired( ) || continue_on_result_destruction;
			}

			template<typename... NewArgs>
			auto next_package( NewArgs... nargs ) {
				return make_shared_package( continue_on_result_destruction, std::move( m_result ), std::move( f_list ), std::move( nargs )... );
			}

			package_t( bool continueonclientdestruction, result_t result, functions_t functions, Args... args ):
				f_list { std::move( functions ) },
				targs { std::make_tuple( std::move( args )... ) },
				m_result { result },
				continue_on_result_destruction { continueonclientdestruction } { }
		};	// package_t

		template<typename Result, typename Functions, typename... Args>
		auto make_package( bool continue_on_result_destruction, Result && result, Functions && functions, Args&&... args ) {
			return package_t<Result, Functions, Args...>{ continue_on_result_destruction, std::forward<Result>( result ), std::forward<Functions>( functions ), std::forward<Args>( args )... };
		}

		template<typename Result, typename Functions, typename... Args>
		auto make_shared_package( bool continue_on_result_destruction, Result && result, Functions && functions, Args&&... args ) {
			return std::make_shared<package_t<Result, Functions, Args...>>( make_package( continue_on_result_destruction, std::forward<Result>( result ), std::forward<Functions>( functions ), std::forward<Args>( args )... ) );
		}

		template<size_t pos, typename TFunctions, typename Arg>
		auto function_composer_impl( TFunctions funcs, last_function_tag, Arg&& arg ) {
			static_assert( pos == std::tuple_size<TFunctions>::value - 1, "last_function_tag should only be retuned for last item in tuple" );
			auto func = std::get<pos>( funcs );
			return func( std::forward<Arg>( arg ) );
		}

		template<size_t pos, typename TFunctions, typename... Args>
		auto function_composer_impl( TFunctions funcs, function_tag, Args&&... args ) {
			static_assert( pos < std::tuple_size<TFunctions>::value - 1, "function_tag should only be retuned for all but last item in tuple" );
			auto func = std::get<pos>( funcs );
			static auto const next_pos = pos + 1;
			return function_composer_impl<next_pos>( funcs, typename which_type_t<next_pos, TFunctions>::category{ }, func( std::forward<Args>( args )... ) );
		}

		template<typename... Functions>
		struct function_composer_t {
			using tfunction_t = std::tuple<std::decay_t<Functions>...>;
			tfunction_t funcs;

			constexpr function_composer_t( Functions&&... fs ):
				funcs { std::make_tuple( fs... ) } { }

			template<typename... Args>
			auto apply( Args&&... args ) {
				return function_composer_impl<0>( funcs, typename which_type_t<0, tfunction_t>::category { }, std::forward<Args>( args )... );
			}
		};
	}	// namespace impl

	template<typename... Functions>
	class function_stream {
		static_assert(sizeof...(Functions) > 0, "Must pass more than 0 items");
		using function_t = std::tuple<std::decay_t<Functions>...>;
		using func_comp_t = impl::function_composer_t<Functions...>;

		function_t m_funcs;
	public:
		bool continue_on_result_destruction;

		constexpr function_stream( Functions... funcs ):
			m_funcs { std::make_tuple( std::move( funcs )... ) },
			continue_on_result_destruction { true } { }

		template<typename... Args>
		auto operator( )( Args... args ) const {
			using func_result_t = decltype(std::declval<func_comp_t>( ).apply( args... ));
			future_result_t<func_result_t> result;
			impl::call<0>( impl::make_shared_package( continue_on_result_destruction, result.weak_ptr( ), m_funcs, std::move( args )... ) );
			return result;
		}
	};	// function_stream

	template<typename... Functions>
	constexpr auto make_function_stream( Functions&&... funcs ) {
		return function_stream<Functions...>{ std::forward<Functions>( funcs )... };
	}

	template<typename T, typename... Ts>
	std::vector<T> create_vector( T && value, Ts&&... values ) {
		return std::vector<T>{ std::initializer_list<T>{ std::forward<T>( value ), std::forward<Ts>( values )...} };
	}
}    // namespace daw
