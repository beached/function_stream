// The MIT License (MIT)
//
// Copyright (c) Darrell Wright
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

#include "daw_fs_concepts.h"
#include "future_result.h"
#include "impl/function_stream_impl.h"
#include "package.h"
#include "task_scheduler.h"

#include <daw/cpp_17.h>

#include <tuple>
#include <type_traits>
#include <utility>

namespace daw {
	template<typename... Functions>
	class function_stream {
		using function_t = std::tuple<std::remove_cvref_t<Functions>...>;
		using func_comp_t = impl::function_composer_t<Functions...>;

		function_t m_funcs;

	public:
		bool continue_on_result_destruction = true;

		template<not_me<function_stream> F, typename... Fs>
		  requires(
		    daw::all_true_v<
		      not std::is_same_v<std::tuple<Functions...>, std::remove_cvref_t<F>>,
		      not daw::any_true_v<std::is_function_v<F>,
		                          std::is_function_v<Fs>...>> )
		explicit constexpr function_stream( F &&f, Fs &&...funcs )
		  : m_funcs( DAW_FWD( f ), DAW_FWD( funcs )... ) {}

		explicit constexpr function_stream( std::tuple<Functions...> &&funcs )
		  : m_funcs( DAW_MOVE( funcs ) ) {}

		explicit constexpr function_stream( std::tuple<Functions...> const &funcs )
		  : m_funcs( funcs ) {}

		template<typename... Args>
		[[nodiscard]] auto operator( )( Args &&...args ) const {
			using func_result_t =
			  decltype( std::declval<func_comp_t>( ).apply( args... ) );
			future_result_t<func_result_t> result{ };
			impl::call<0>( make_shared_package( continue_on_result_destruction,
			                                    result.get_handle( ), m_funcs,
			                                    DAW_FWD( args )... ) );
			return result;
		}
	}; // function_stream

	template<typename... Functions>
	function_stream( Functions &&... )
	  -> function_stream<std::remove_cvref_t<Functions>...>;

	template<typename... Functions>
	function_stream( std::tuple<Functions> &&... )
	  -> function_stream<std::remove_cvref_t<Functions>...>;

	template<typename... Functions>
	function_stream( std::tuple<Functions> const &... )
	  -> function_stream<std::remove_cvref_t<Functions>...>;

	template<typename T>
	concept Waitable = requires( T &&waitable ) { waitable.wait( ); };

	template<typename... Functions>
	constexpr auto make_function_stream( Functions &&...funcs ) noexcept {
		return function_stream( std::make_tuple( daw::make_callable( funcs )... ) );
	}

	template<Waitable... Waitables>
	void wait_for_function_streams( Waitables &&...waitables ) {
		( ( (void)waitables.wait( ) ), ... );
	}

	template<typename... Funcs>
	class future_generator_t {
		template<typename...>
		friend class daw::future_generator_t;

		std::tuple<Funcs...> m_funcs;

		template<typename... Functions>
		[[nodiscard]] static constexpr future_generator_t<Functions...>
		make_future_generator( std::tuple<Functions...> &&tp_funcs ) {
			return future_generator_t<Functions...>{ DAW_MOVE( tp_funcs ) };
		}

		template<typename... Functions>
		[[nodiscard]] static constexpr future_generator_t<Functions...>
		make_future_generator( std::tuple<Functions...> const &tp_funcs ) {
			return future_generator_t<Functions...>{ tp_funcs };
		}

	public:
		template<typename... Functions>
		  requires(
		    sizeof...( Functions ) > 1 &&
		    sizeof...( Functions ) == sizeof...( Funcs ) &&
		    not std::is_same_v<std::tuple<Funcs...>, std::tuple<Functions...>> )
		explicit constexpr future_generator_t( Functions &&...funcs ) noexcept(
		  noexcept( std::tuple<Funcs...>{ DAW_FWD( funcs )... } ) )
		  : m_funcs{ DAW_FWD( funcs )... } {}

		explicit constexpr future_generator_t(
		  std::tuple<Funcs...> const &tp_funcs )
		  : m_funcs{ tp_funcs } {}

		explicit constexpr future_generator_t( std::tuple<Funcs...> &&tp_funcs )
		  : m_funcs{ DAW_MOVE( tp_funcs ) } {}

		template<typename... Args>
		[[nodiscard]] constexpr decltype( auto )
		operator( )( Args &&...args ) const {
			return get_function_stream( )( DAW_FWD( args )... );
		}

		[[nodiscard]] constexpr function_stream<Funcs...>
		get_function_stream( ) const {
			return function_stream<Funcs...>( m_funcs );
		}

		template<typename... NextFunctions>
		[[nodiscard]] constexpr auto
		next( NextFunctions &&...next_functions ) const {
			return make_future_generator( std::tuple_cat(
			  m_funcs, std::make_tuple( DAW_FWD( next_functions )... ) ) );
		}

		template<typename... NextFuncs>
		[[nodiscard]] constexpr decltype( auto )
		join( future_generator_t<NextFuncs...> const &fut2 ) const {
			return make_future_generator( std::tuple_cat( m_funcs, fut2.m_funcs ) );
		}
	};

	template<typename... Functions>
	[[nodiscard]] constexpr future_generator_t<Functions...>
	make_future_generator( Functions &&...functions ) {
		return future_generator_t<Functions...>{ DAW_FWD( functions )... };
	}

	namespace impl {
		// clang-format off
		template<typename T, typename U>
		constexpr bool can_next = requires( T && t, U && u ) { t.next( u ); };
		// clang-format on
	} // namespace impl

	// clang-format off
	template<typename NextFunction, typename... Functions>
	requires( impl::can_next<future_generator_t<Functions...>, NextFunction> )
  [[nodiscard]] constexpr decltype( auto )
	operator|( future_generator_t<Functions...> const &lhs,
						 NextFunction &&next_func ) {
		// clang-format on
		return lhs.next( DAW_FWD( next_func ) );
	}

	namespace impl {
		// clang-format off
		template<typename T, typename Us>
		constexpr bool can_join = requires( T && t, Us && u) { t.join( u ); };
		// clang-format on
	} // namespace impl

	// clang-format off
	template<typename... Functions, typename... Functions2>
	  requires( impl::can_join<future_generator_t<Functions...>,
	                           future_generator_t<Functions2...>> )
	[[nodiscard]] constexpr decltype( auto )
	operator|( future_generator_t<Functions...> const &lhs,
	           future_generator_t<Functions2...> const &rhs ) {
		// clang-format on
		return lhs.join( rhs );
	}

	template<typename... Functions>
	[[nodiscard]] constexpr auto compose_functions( Functions &&...functions ) {
		return impl::function_composer_t<std::remove_cv_t<Functions>...>{
		  DAW_FWD( functions )... };
	}

	template<typename... Functions>
	[[nodiscard]] constexpr auto compose( Functions &&...funcs ) noexcept {
		return impl::function_composer_t<std::remove_cv_t<Functions>...>(
		  DAW_FWD( funcs )... );
	}

	template<typename... Functions>
	[[nodiscard]] constexpr auto compose_future( Functions &&...funcs ) noexcept {
		return future_generator_t<std::remove_cv_t<Functions>...>(
		  std::tuple<std::remove_cv_t<Functions>...>( DAW_FWD( funcs )... ) );
	}
} // namespace daw
