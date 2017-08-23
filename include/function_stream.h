// The MIT License (MIT)
//
// Copyright (c) 2017-2017 Darrell Wright
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
#include "package.h"
#include "task_scheduler.h"

#include <daw/cpp_17.h>

namespace daw {
	namespace impl {
		template<size_t S, typename Tuple>
		using is_function_tag = daw::bool_constant < 0 <= S &&S<std::tuple_size<std::decay_t<Tuple>>::value>;

		template<size_t S, typename Tuple>
		constexpr bool const is_function_tag_v = is_function_tag<S, Tuple>::value;

		template<size_t S, typename Tuple>
		using is_function_tag_t = typename is_function_tag<S, Tuple>::type;

		template<size_t pos, typename Package>
		struct call_task_t;
		template<typename Package>
		struct call_task_last_t;

		struct function_tag {
			using category = function_tag;
		};
		struct last_function_tag {
			using category = last_function_tag;
		};

		template<size_t S, typename Tuple>
		using which_type_t = typename std::conditional <
		                     S<std::tuple_size<std::decay_t<Tuple>>::value - 1, function_tag, last_function_tag>::type;

		template<size_t pos, typename Package>
		void call_task( Package, last_function_tag );
		template<size_t pos, typename Package>
		void call_task( Package, function_tag );

		template<size_t pos, typename Package>
		void call( Package package ) {
			get_task_scheduler( ).add_task( [p = std::move( package )]( ) mutable {
				call_task<pos>( std::move( p ), typename impl::which_type_t<pos, decltype( p->function_list( ) )>::category{} );
			} );
		}

		template<size_t pos, typename Package>
		void call_task( Package package, last_function_tag ) {
			if( !package->continue_processing( ) ) {
				return;
			}
			auto func = std::get<pos>( package->function_list( ) );
			auto client_data = package->result( ).lock( );
			if( client_data ) {
				client_data->from_code( [&]( ) mutable { return daw::apply( func, std::move( package->targs( ) ) ); } );
			} else {
				daw::apply( func, std::move( package->targs( ) ) );
			}
		}

		template<size_t pos, typename Package>
		void call_task( Package package, function_tag ) {
			if( !package->continue_processing( ) ) {
				return;
			}
			auto func = std::get<pos>( package->function_list( ) );
			try {
				static size_t const new_pos = pos + 1;
				call<new_pos>( package->next_package( daw::apply( func, std::move( package->targs( ) ) ) ) );
			} catch( ... ) {
				auto result = package->result( ).lock( );
				if( result ) {
					result->from_exception( std::current_exception( ) );
				}
			}
		}

		template<size_t pos, typename TFunctions, typename Arg>
		auto function_composer_impl( TFunctions funcs, last_function_tag, Arg &&arg ) {
			static_assert( pos == std::tuple_size<TFunctions>::value - 1,
			               "last_function_tag should only be retuned for last item in tuple" );
			auto func = std::get<pos>( funcs );
			return func( std::forward<Arg>( arg ) );
		}

		template<size_t pos, typename TFunctions, typename... Args>
		auto function_composer_impl( TFunctions funcs, function_tag, Args &&... args ) {
			static_assert( pos < std::tuple_size<TFunctions>::value - 1,
			               "function_tag should only be retuned for all but last item in tuple" );
			auto func = std::get<pos>( funcs );
			static auto const next_pos = pos + 1;
			return function_composer_impl<next_pos>( funcs, typename which_type_t<next_pos, TFunctions>::category{},
			                                         func( std::forward<Args>( args )... ) );
		}

		template<typename... Functions>
		struct function_composer_t {
			using tfunction_t = std::tuple<std::decay_t<Functions>...>;
			tfunction_t funcs;

			constexpr function_composer_t( Functions &&... fs ) : funcs{std::make_tuple( fs... )} {}

			template<typename... Args>
			auto apply( Args &&... args ) {
				return function_composer_impl<0>( funcs, typename which_type_t<0, tfunction_t>::category{},
				                                  std::forward<Args>( args )... );
			}
		};
	} // namespace impl

	template<typename... Functions>
	class function_stream {
		static_assert( sizeof...( Functions ) > 0, "Must pass more than 0 items" );
		using function_t = std::tuple<std::decay_t<Functions>...>;
		using func_comp_t = impl::function_composer_t<Functions...>;

		function_t m_funcs;

	  public:
		bool continue_on_result_destruction;

		constexpr function_stream( Functions... funcs )
		    : m_funcs{std::make_tuple( std::move( funcs )... )}, continue_on_result_destruction{true} {}

		template<typename... Args>
		auto operator( )( Args... args ) const {
			using func_result_t = decltype( std::declval<func_comp_t>( ).apply( args... ) );
			future_result_t<func_result_t> result;
			impl::call<0>( make_shared_package( continue_on_result_destruction, result.weak_ptr( ), m_funcs,
			                                    std::move( args )... ) );
			return result;
		}
	}; // function_stream

	template<typename... Functions>
	constexpr auto make_function_stream( Functions &&... funcs ) {
		return function_stream<Functions...>{std::forward<Functions>( funcs )...};
	}

	template<typename FunctionStream>
	void wait_for_function_streams( FunctionStream & function_stream ) {
		function_stream->wait( );
	}

	template<typename... FunctionStreams>
	void wait_for_function_streams( FunctionStreams&... function_streams ) {
		[](...){ }((wait_for_function_streams( function_streams ), 0)...);
	}

	template<typename T, typename... Ts>
	std::vector<T> create_vector( T &&value, Ts &&... values ) {
		return std::vector<T>{std::initializer_list<T>{std::forward<T>( value ), std::forward<Ts>( values )...}};
	}
} // namespace daw
