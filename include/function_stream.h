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

#include <boost/any.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <tuple>
#include <vector>

#include <daw/daw_expected.h>
#include "task_scheduler.h"

namespace daw {
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
		constexpr auto const is_function_tag_v = is_function_tag<S, Tuple>::value;

		template<size_t S, typename Tuple>
		using is_function_tag_t = typename is_function_tag<S, Tuple>::type;

		template<size_t pos, typename TFunctions, typename Callback, typename TArgs> struct call_task_t;
		template<typename Callback, typename TArgs> struct call_task_last_t;

		struct function_tag { using category = function_tag; };
		struct callback_tag { using category = callback_tag; };

		template<size_t pos, typename T>
		struct which_type: public std::conditional<is_function_tag_v<pos, T>, function_tag, callback_tag> { };

		template<size_t pos, typename T>
		using which_type_t = typename which_type<pos, T>::type;

		template<size_t pos, typename TFunctions, typename Callback, typename TArgs>
		auto call( TFunctions tfuncs, Callback cb, TArgs args, function_tag const & ) {
			get_task_scheduler( ).add_task( call_task_t<pos, TFunctions, Callback, TArgs>{ tfuncs, std::move( cb ), std::move( args ) } );
		}

		template<size_t pos, typename TFunctions, typename Callback, typename TArgs>
		auto call( TFunctions tfuncs, Callback cb, TArgs args, callback_tag const & ) { 
			get_task_scheduler( ).add_task( call_task_last_t<Callback, TArgs>{ std::move( cb ), std::move( args ) } );
		}

		template<size_t pos, typename TFunctions, typename Callback, typename TArgs>
		struct call_task_t {
			TFunctions m_tfuncs;
			Callback m_cb;
			TArgs m_targs;

			constexpr call_task_t( TFunctions tfuncs, Callback cb, TArgs targs ):
				m_tfuncs { std::move( tfuncs ) },
				m_cb { std::move( cb ) },
				m_targs { std::move( targs ) } { }

			call_task_t( ) = delete;
			~call_task_t( ) = default;
			call_task_t( call_task_t const & ) = default;
			call_task_t & operator=( call_task_t const & ) = default;
			call_task_t( call_task_t && ) = default;
			call_task_t & operator=( call_task_t && ) = default;

			void operator( )( ) {
				auto const func = std::get<pos>( m_tfuncs );
				auto result = std::make_tuple( apply_tuple( func, std::move( m_targs ) ) );
				static size_t const new_pos = pos + 1;
				call<new_pos>( std::move( m_tfuncs ), std::move( m_cb ), std::move( result ), typename which_type_t<new_pos, decltype(m_tfuncs)>::category{ } );
			}
		};	// call_task_t

		template<typename Callback, typename TArg>
		struct call_task_last_t {
			Callback m_cb;
			TArg m_targ;

			constexpr call_task_last_t( Callback cb, TArg targ ):
				m_cb { std::move( cb ) },
				m_targ { std::move( targ ) } { }

			call_task_last_t( ) = delete;
			~call_task_last_t( ) = default;
			call_task_last_t( call_task_last_t const & ) = default;
			call_task_last_t & operator=( call_task_last_t const & ) = default;
			call_task_last_t( call_task_last_t && ) = default;
			call_task_last_t & operator=( call_task_last_t && ) = default;

			void operator( )( ) {
				apply_tuple( m_cb, std::move( m_targ ) );
			}
		};	// call_task_last_t
	}	// namespace impl

	template<typename... Functions>
	class function_stream {
		static_assert(sizeof...(Functions) > 0, "Must supply at least 1 Functor");
		std::tuple<std::decay_t<Functions>...> m_funcs;

	public:
		function_stream( Functions... funcs ):
			m_funcs { std::make_tuple( std::move( funcs )... ) } { }

		template<typename Callback, typename... Args>
		void operator( )( Callback && cb, Args&&... args ) const {
			using t_type = std::tuple<Functions...>;
			impl::call<0>( m_funcs, std::move( cb ), std::make_tuple( std::move( args )... ), typename impl::which_type_t<0, t_type>::category{ } );
		}
	};	// function_stream

	template<typename... Functions>
	constexpr auto make_function_stream( Functions&&... funcs ) {
		return function_stream<Functions...>{ std::forward<Functions>( funcs )... };
	}
}    // namespace daw
