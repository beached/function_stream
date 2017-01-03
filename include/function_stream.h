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

		template<size_t pos, typename TFunctions, typename OnSuccess, typename OnError, typename TArgs> struct call_task_t;
		template<typename OnSuccess, typename OnError, typename TArgs> struct call_task_last_t;

		struct function_tag { using category = function_tag; };
		struct callback_tag { using category = callback_tag; };

		template<size_t pos, typename T>
		struct which_type: public std::conditional<is_function_tag_v<pos, T>, function_tag, callback_tag> { };

		template<size_t pos, typename T>
		using which_type_t = typename which_type<pos, T>::type;

		template<size_t pos, typename TFunctions, typename OnSuccess, typename OnError, typename TArgs>
		constexpr void call( TFunctions tfuncs, OnSuccess on_success, OnError on_error, TArgs args, function_tag const & ) {
			get_task_scheduler( ).add_task( call_task_t<pos, TFunctions, OnSuccess, OnError, TArgs>{ tfuncs, std::move( on_success ), std::move( on_error ), std::move( args ) } );
		}

		template<size_t pos, typename TFunctions, typename OnSuccess, typename OnError, typename TArgs>
		constexpr void call( TFunctions tfuncs, OnSuccess on_success, OnError on_error, TArgs args, callback_tag const & ) { 
			get_task_scheduler( ).add_task( call_task_last_t<OnSuccess, OnError, TArgs>{ std::move( on_success ), std::move( on_error ), std::move( args ) } );
		}

		template<size_t pos, typename TFunctions, typename OnSuccess, typename OnError, typename TArgs>
		struct call_task_t {
			TFunctions m_tfuncs;
			OnSuccess m_on_success;
			OnError m_on_error;
			TArgs m_targs;

			constexpr call_task_t( TFunctions tfuncs, OnSuccess on_success, OnError on_error, TArgs targs ):
				m_tfuncs { std::move( tfuncs ) },
				m_on_success{ std::move( on_success ) },
				m_on_error{ std::move( on_error ) },
				m_targs { std::move( targs ) } { }

			call_task_t( ) = delete;
			~call_task_t( ) = default;
			call_task_t( call_task_t const & ) = default;
			call_task_t & operator=( call_task_t const & ) = default;
			call_task_t( call_task_t && ) = default;
			call_task_t & operator=( call_task_t && ) = default;

			void operator( )( ) {
				auto const func = std::get<pos>( m_tfuncs );
				try {
					auto result = std::make_tuple( apply_tuple( func, std::move( m_targs ) ) );
					static size_t const new_pos = pos + 1;
					call<new_pos>( std::move( m_tfuncs ), std::move( m_on_success ), m_on_error, std::move( result ), typename which_type_t<new_pos, decltype(m_tfuncs)>::category { } );
				} catch(...) {
					m_on_error( ErrorException<pos>{ std::current_exception( ) } );
				}
			}
		};	// call_task_t

		template<typename OnSuccess, typename OnError, typename TArg>
		struct call_task_last_t {
			OnSuccess m_on_success;
			OnError m_on_error;
			TArg m_targ;

			constexpr call_task_last_t( OnSuccess on_success, OnError on_error, TArg targ ):
				m_on_success{ std::move( on_success ) },
				m_on_error { std::move( on_error ) },
				m_targ { std::move( targ ) } { }

			call_task_last_t( ) = delete;
			~call_task_last_t( ) = default;
			call_task_last_t( call_task_last_t const & ) = default;
			call_task_last_t & operator=( call_task_last_t const & ) = default;
			call_task_last_t( call_task_last_t && ) = default;
			call_task_last_t & operator=( call_task_last_t && ) = default;

			void operator( )( ) {
				try {
					apply_tuple( m_on_success, std::move( m_targ ) );
				} catch(...) {
					m_on_error( ErrorException<std::numeric_limits<size_t>::max( )>{ std::current_exception( ) } );
				}
			}
		};	// call_task_last_t
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
			struct package_t {
				function_t f_list;
				OnSuccess f_success;
				OnError f_error;
				using arguments_t = std::tuple<Args...>;
				arguments_t targs;

				package_t( ) = delete;
				~package_t( ) = default;
				package_t( package_t const & ) = delete;
				package_t( package_t && ) = default;
				package_t & operator=( package_t const & ) = delete;
				package_t & operator=( package_t && ) = default;

				package_t( function_t functions, OnSuccess suc, OnError err, arguments_t arg ):
					f_list{ std::move( functions ) },
					f_success{ std::move( suc ) },
					f_error{ std::move( err ) },
					targs{ std::move( arg ) } { }
			};

			using t_type = std::tuple<Functions...>;
			impl::call<0>( m_funcs, std::move( on_success ), std::move( on_error ), std::make_tuple( std::move( args )... ), typename impl::which_type_t<0, t_type>::category{ } );
		}
	};	// function_stream

	template<typename... Functions>
	constexpr auto make_function_stream( Functions&&... funcs ) {
		return function_stream<Functions...>{ std::forward<Functions>( funcs )... };
	}
}    // namespace daw
