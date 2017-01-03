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
#include <iostream>
#include <tuple>

#include "task_scheduler.h"

namespace daw {
	struct Error {
		Error( ) = default;
		virtual ~Error( ); 
		Error( Error const & ) = default;
		Error( Error && ) = default;
		Error & operator=( Error const & ) = default;
		Error & operator=( Error && ) = default;
	};

	template<size_t pos>
	struct ErrorException: public Error {
		std::exception_ptr ex_ptr;

		ErrorException( ) = delete;

		explicit ErrorException( std::exception_ptr ptr ):
				ex_ptr{ std::move( ptr ) } {  }

		~ErrorException( ) = default;
		ErrorException( ErrorException const & ) = default;
		ErrorException( ErrorException && ) = default;
		ErrorException & operator=( ErrorException const & ) = default;
		ErrorException & operator=( ErrorException && ) = default;

		static constexpr auto function_index( ) noexcept {
			return pos;
		}

		void get_exception( ) const {
			std::rethrow_exception( ex_ptr );
		}		
	};

	namespace impl {
		template<typename Function, typename Tuple, size_t ...S>
		auto apply_tuple( Function func, Tuple && t, std::index_sequence<S...> ) {
			return func( std::forward<decltype(std::get<S>( t ))>( std::get<S>( t ) )... );
		}

		template<typename Function, typename Tuple, typename Index = std::make_index_sequence<std::tuple_size<Tuple>::value>>
		auto apply_tuple( Function func, Tuple && t ) {
			return apply_tuple( func, std::forward<Tuple>( t ), Index { } );
		}

		template<size_t S, typename Tuple>
		using is_function_tag = std::integral_constant<bool, 0<=S && S < std::tuple_size<Tuple>::value>;
		
		template<size_t S, typename Tuple>
		constexpr bool const is_function_tag_v = is_function_tag<S, Tuple>::value;

		template<size_t S, typename Tuple>
		using is_function_tag_t = typename is_function_tag<S, Tuple>::type;

		template<size_t pos, typename Package> struct call_task_t;
		template<typename Package> struct call_task_last_t;

		struct function_tag { using category = function_tag; };
		struct callback_tag { using category = callback_tag; };

		template<size_t pos, typename T>
		struct which_type: public std::conditional<is_function_tag_v<pos, T>, function_tag, callback_tag> { };

		template<size_t pos, typename T>
		using which_type_t = typename which_type<pos, T>::type;

		template<size_t pos, typename Package>
		constexpr void call( Package package, function_tag const & ) { 
			get_task_scheduler( ).add_task( call_task_t<pos, Package>{ std::move( package ) } );
		}

		template<size_t pos, typename Package>
		constexpr void call( Package package, callback_tag const & ) { 
			get_task_scheduler( ).add_task( call_task_last_t<Package>{ std::move( package ) } );
		}

		template<size_t pos, typename Package>
		struct call_task_t {
			Package m_package;

			constexpr call_task_t( Package package ):
				m_package { std::move( package ) } { }

			call_task_t( ) = delete;
			~call_task_t( ) = default;
			call_task_t( call_task_t const & ) = default;
			call_task_t & operator=( call_task_t const & ) = default;
			call_task_t( call_task_t && ) = default;
			call_task_t & operator=( call_task_t && ) = default;

			void operator( )( ) {
				auto const func = std::get<pos>( m_package->f_list );
				try {
					std::clog << "A\n";
					auto result = apply_tuple( func, std::move( m_package->targs ) );
					static size_t const new_pos = pos + 1;
					call<new_pos>( m_package->next_package( std::move( result ) ), typename which_type_t<new_pos, decltype(m_package->f_list)>::category { } );
				} catch(...) {
					m_package->f_error( ErrorException<pos>{ std::current_exception( ) } );
				}
			}
		};	// call_task_t

		template<typename Package>
		struct call_task_last_t {
			Package m_package;

			constexpr call_task_last_t( Package package ):
				m_package{ std::move( package ) } { }

			call_task_last_t( ) = delete;
			~call_task_last_t( ) = default;
			call_task_last_t( call_task_last_t const & ) = default;
			call_task_last_t & operator=( call_task_last_t const & ) = default;
			call_task_last_t( call_task_last_t && ) = default;
			call_task_last_t & operator=( call_task_last_t && ) = default;

			void operator( )( ) {
				try {
					std::clog << "B\n";
					apply_tuple( m_package->f_success, std::move( m_package->targs ) );
				} catch(...) {
					m_package->f_error( ErrorException<std::numeric_limits<size_t>::max( )>{ std::current_exception( ) } );
				}
			}
		};	// call_task_last_t

		template<typename Functions, typename OnSuccess, typename OnError, typename... Args>
		struct package_t {
			using functions_t = Functions;
			using on_success_t = OnSuccess;
			using on_error_t = OnError;
			using arguments_t = std::tuple<Args...>;
			
			functions_t f_list;
			on_success_t f_success;
			on_error_t f_error;
			arguments_t targs;

			package_t( ) = delete;
			~package_t( ) = default;
			package_t( package_t const & ) = delete;
			package_t( package_t && ) = default;
			package_t & operator=( package_t const & ) = delete;
			package_t & operator=( package_t && ) = default;

			template<typename... NewArgs>
			auto next_package( NewArgs... nargs ) {
				return std::make_shared<package_t<functions_t, on_success_t, on_error_t, NewArgs...>>( std::move( f_list ), std::move( f_success ), std::move( f_error ), std::move( nargs )... );
			}

			package_t( functions_t functions, on_success_t on_success, on_error_t on_error, Args... args ):
				f_list{ std::move( functions ) },
				f_success{ std::move( on_success ) },
				f_error{ std::move( on_error ) },
				targs{ std::make_tuple( std::move( args )... ) } { }
		};


	}	// namespace impl

	template<typename... Functions>
	class function_stream {
		static_assert(sizeof...(Functions) > 0, "Must pass more than 0 items");
		using function_t = std::tuple<std::decay_t<Functions>...>;
		function_t m_funcs;

	public:
		constexpr function_stream( Functions... funcs ):
			m_funcs { std::make_tuple( std::move( funcs )... ) } { }

		template<typename OnSuccess, typename OnError, typename... Args>
		constexpr void operator( )( OnSuccess on_success, OnError on_error, Args... args ) const {
			impl::call<0>( std::make_shared<impl::package_t<function_t, OnSuccess, OnError, Args...>>( m_funcs, std::move( on_success ), std::move( on_error ), std::move( args )... ), typename impl::which_type_t<0, function_t>::category{ } );
		}
	};	// function_stream

	template<typename... Functions>
	constexpr auto make_function_stream( Functions&&... funcs ) {
		return function_stream<Functions...>{ std::forward<Functions>( funcs )... };
	}
}    // namespace daw
