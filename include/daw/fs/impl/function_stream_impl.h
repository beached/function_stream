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

#include <tuple>
#include <utility>

#include <daw/cpp_17.h>

#include "../task_scheduler.h"

namespace daw {
	namespace impl {
		template<size_t S, typename Tuple>
		using is_function_tag = daw::bool_constant<(
		  0 <= S && S < daw::tuple_size_v<std::decay_t<Tuple>> )>;

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
		using which_type_t =
		  typename std::conditional<( S <
		                              daw::tuple_size_v<std::decay_t<Tuple>> - 1 ),
		                            function_tag, last_function_tag>::type;

		template<size_t pos, typename Package>
		void call_task( Package &&, last_function_tag );

		template<size_t pos, typename Package>
		void call_task( Package &&, function_tag );

		template<size_t pos, typename Package>
		void call( Package package ) {
			get_task_scheduler( ).add_task( [p = std::move( package )]( ) mutable {
				call_task<pos>( std::move( p ),
				                typename impl::which_type_t<
				                  pos, decltype( p->function_list( ) )>::category{} );
			} );
		}

		template<size_t pos, typename Package>
		void call_task( Package &&package, last_function_tag ) {
			if( !package->continue_processing( ) ) {
				return;
			}
			auto func = std::get<pos>( package->function_list( ) );
			auto client_data = package->result( ).lock( );
			if( client_data ) {
				client_data->from_code( [&]( ) mutable {
					return daw::apply( func, std::move( package->targs( ) ) );
				} );
			} else {
				daw::apply( func, std::move( package->targs( ) ) );
			}
		}

		template<size_t pos, typename Package>
		void call_task( Package &&package, function_tag ) {
			if( !package->continue_processing( ) ) {
				return;
			}
			auto func = std::get<pos>( package->function_list( ) );
			try {
				call<pos + 1>( package->next_package(
				  daw::apply( func, std::move( package->targs( ) ) ) ) );
			} catch( ... ) {
				auto result = package->result( ).lock( );
				if( result ) {
					result->set_exception( std::current_exception( ) );
				}
			}
		}

		template<size_t pos, typename TFunctions, typename Arg>
		constexpr decltype( auto )
		function_composer_impl( TFunctions funcs, last_function_tag, Arg &&arg ) {
			static_assert(
			  pos == daw::tuple_size_v<TFunctions> - 1,
			  "last_function_tag should only be retuned for last item in tuple" );

			auto const func = std::get<pos>( std::forward<TFunctions>( funcs ) );
			return func( std::forward<Arg>( arg ) );
		}

		template<size_t pos, typename TFunctions, typename... Args,
		         typename = std::enable_if_t<daw::tuple_size_v<TFunctions> == 0>>
		constexpr void function_composer_impl( TFunctions, function_tag,
		                                       Args &&... ) noexcept {}

		template<size_t pos, typename TFunctions, typename... Args,
		         typename = std::enable_if_t<daw::tuple_size_v<TFunctions> != 0>>
		constexpr decltype( auto )
		function_composer_impl( TFunctions funcs, function_tag, Args &&... args ) {
			static_assert(
			  pos < daw::tuple_size_v<TFunctions> - 1,
			  "function_tag should only be retuned for all but last item in tuple" );
			auto const func = std::get<pos>( funcs );

			// If this crashes here, the next function probably does not take as
			// arugment the result of the previous function
			return function_composer_impl<pos + 1>(
			  funcs, typename which_type_t<pos + 1, TFunctions>::category{},
			  func( std::forward<Args>( args )... ) );
		}

		template<typename... Functions>
		struct function_composer_t {
			using tfunction_t = std::tuple<std::decay_t<Functions>...>;
			tfunction_t funcs;

			constexpr explicit function_composer_t( Functions &&... fs ) noexcept
			  : funcs{std::make_tuple( fs... )} {}
			constexpr explicit function_composer_t( tfunction_t functions ) noexcept
			  : funcs{std::move( functions )} {}

		private:
			template<typename... Fs>
			static constexpr function_composer_t<Fs...>
			make_function_composer_t( std::tuple<Fs...> tpfuncs ) noexcept {
				return function_composer_t<Fs...>{std::move( tpfuncs )};
			}

		public:
			template<typename... Args>
			constexpr decltype( auto ) apply( Args &&... args ) const {
				return function_composer_impl<0>(
				  funcs, typename which_type_t<0, tfunction_t>::category{},
				  std::forward<Args>( args )... );
			}

			template<typename... Args>
			constexpr decltype( auto ) operator( )( Args &&... args ) const {
				return apply( std::forward<Args>( args )... );
			}

			template<typename... NextFunctions>
			constexpr decltype( auto )
			next( NextFunctions &&... next_functions ) const noexcept {
				return make_function_composer_t(
				  std::tuple_cat( funcs, std::make_tuple( std::forward<NextFunctions>(
				                           next_functions )... ) ) );
			}
		};

		template<typename NextFunction, typename... Functions>
		constexpr decltype( auto )
		operator|( function_composer_t<Functions...> const &lhs,
		           NextFunction &&next_func ) noexcept {
			return lhs.next( std::forward<NextFunction>( next_func ) );
		}
	} // namespace impl
} // namespace daw