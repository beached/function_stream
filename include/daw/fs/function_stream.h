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

#include "future_result.h"
#include "package.h"
#include "task_scheduler.h"

#include <daw/cpp_17.h>

#include "impl/function_stream_impl.h"

namespace daw {
	template<typename... Functions>
	class function_stream {
		static_assert( sizeof...( Functions ) > 0, "Must pass more than 0 items" );
		using function_t = std::tuple<std::decay_t<Functions>...>;
		using func_comp_t = impl::function_composer_t<Functions...>;

		function_t m_funcs;

	public:
		bool continue_on_result_destruction;

		constexpr explicit function_stream( Functions &&... funcs )
		  : m_funcs{std::make_tuple( daw::move( funcs )... )}
		  , continue_on_result_destruction{true} {}

		constexpr explicit function_stream( Functions const &... funcs )
		  : m_funcs{std::make_tuple( funcs... )}
		  , continue_on_result_destruction{true} {}

		constexpr explicit function_stream( std::tuple<Functions...> &&tpfuncs )
		  : m_funcs{daw::move( tpfuncs )}
		  , continue_on_result_destruction{true} {}

		constexpr explicit function_stream(
		  std::tuple<Functions...> const &tpfuncs )
		  : m_funcs{tpfuncs}
		  , continue_on_result_destruction{true} {}

		template<typename... Args>
		auto operator( )( Args &&... args ) const {
			using func_result_t =
			  decltype( std::declval<func_comp_t>( ).apply( args... ) );
			future_result_t<func_result_t> result{};
			impl::call<0>( make_shared_package( continue_on_result_destruction,
			                                    result.weak_ptr( ), m_funcs,
			                                    std::forward<Args>( args )... ) );
			return result;
		}
	}; // function_stream

	template<typename... Functions>
	constexpr function_stream<Functions...>
	make_function_stream( Functions &&... funcs ) {
		return function_stream<Functions...>{std::forward<Functions>( funcs )...};
	}

	template<typename FunctionStream>
	void wait_for_function_streams( FunctionStream &&function_stream ) {
		function_stream.wait( );
	}

	template<typename... FunctionStreams>
	void wait_for_function_streams( FunctionStreams &&... function_streams ) {
		[]( ... ) {}( ( wait_for_function_streams(
		                  std::forward<FunctionStreams>( function_streams ) ),
		                0 )... );
	}

	template<typename... Funcs>
	class future_generator_t {
		template<typename...>
		friend class future_generator_t;

		std::tuple<Funcs...> m_funcs;

		template<typename... Functions>
		constexpr static future_generator_t<Functions...>
		make_future_generator( std::tuple<Functions...> &&tp_funcs ) {
			return future_generator_t<Functions...>{daw::move( tp_funcs )};
		}

		template<typename... Functions>
		constexpr static future_generator_t<Functions...>
		make_future_generator( std::tuple<Functions...> const &tp_funcs ) {
			return future_generator_t<Functions...>{tp_funcs};
		}

	public:
		constexpr future_generator_t( ) noexcept = default;

		template<typename... Functions,
		         std::enable_if_t<(sizeof...( Functions ) > 1 &&
		                           sizeof...( Functions ) == sizeof...( Funcs ) &&
		                           !daw::is_same_v<std::tuple<Funcs...>,
		                                           std::tuple<Functions...>>),
		                          std::nullptr_t> = nullptr>
		constexpr explicit future_generator_t( Functions &&... funcs ) noexcept(
		  noexcept( std::tuple<Funcs...>{std::forward<Functions>( funcs )...} ) )
		  : m_funcs{std::forward<Functions>( funcs )...} {}

		constexpr explicit future_generator_t(
		  std::tuple<Funcs...> const &tp_funcs )
		  : m_funcs{tp_funcs} {}

		constexpr explicit future_generator_t( std::tuple<Funcs...> &&tp_funcs )
		  : m_funcs{daw::move( tp_funcs )} {}

		template<typename... Args>
		constexpr decltype( auto ) operator( )( Args &&... args ) const {
			return get_function_stream( )( std::forward<Args...>( args )... );
		}

		constexpr function_stream<Funcs...> get_function_stream( ) const {
			return function_stream<Funcs...>( m_funcs );
		}

		template<typename... NextFunctions>
		constexpr auto next( NextFunctions &&... next_functions ) const {
			return make_future_generator( std::tuple_cat(
			  m_funcs,
			  std::make_tuple( std::forward<NextFunctions>( next_functions )... ) ) );
		}

		template<typename... NextFuncs>
		constexpr decltype( auto )
		join( future_generator_t<NextFuncs...> const &fut2 ) const {
			return make_future_generator( std::tuple_cat( m_funcs, fut2.m_funcs ) );
		}
	};

	template<typename... Functions>
	constexpr future_generator_t<Functions...>
	make_future_generator( Functions &&... functions ) {
		return future_generator_t<Functions...>{
		  std::forward<Functions>( functions )...};
	}

	namespace impl {
		template<typename T, typename U>
		using detect_can_next =
		  decltype( std::declval<T>( ).next( std::declval<U>( ) ) );

		template<typename T, typename Us>
		constexpr bool can_next = daw::is_detected_v<detect_can_next, T, Us>;
	} // namespace impl

	template<typename NextFunction, typename... Functions,
	         std::enable_if_t<
	           impl::can_next<future_generator_t<Functions...>, NextFunction>,
	           std::nullptr_t> = nullptr>
	constexpr decltype( auto )
	operator|( future_generator_t<Functions...> const &lhs,
	           NextFunction &&next_func ) {
		return lhs.next(
		  std::forward<std::remove_cv_t<NextFunction>>( next_func ) );
	}

	namespace impl {
		template<typename T, typename U>
		using detect_can_join =
		  decltype( std::declval<T>( ).join( std::declval<U>( ) ) );

		template<typename T, typename Us>
		constexpr bool can_join = daw::is_detected_v<detect_can_join, T, Us>;
	} // namespace impl

	template<typename... Functions, typename... Functions2,
	         std::enable_if_t<impl::can_join<future_generator_t<Functions...>,
	                                         future_generator_t<Functions2...>>,
	                          std::nullptr_t> = nullptr>
	constexpr decltype( auto )
	operator|( future_generator_t<Functions...> const &lhs,
	           future_generator_t<Functions2...> const &rhs ) {
		return lhs.join( rhs );
	}

	template<typename... Functions>
	constexpr auto compose_functions( Functions &&... functions ) {
		return impl::function_composer_t<std::remove_cv_t<Functions>...>{
		  std::forward<Functions>( functions )...};
	}

	template<typename... Functions>
	constexpr auto compose( Functions &&... funcs ) noexcept {
		return impl::function_composer_t<std::remove_cv_t<Functions>...>(
		  std::forward<Functions>( funcs )... );
	}

	template<typename... Functions>
	constexpr auto compose_future( Functions &&... funcs ) noexcept {
		return future_generator_t<std::remove_cv_t<Functions>...>(
		  std::tuple<std::remove_cv_t<Functions>...>(
		    std::forward<Functions>( funcs )... ) );
	}

	/*
	template<typename... Funcs>
	class future_generator_split_t {
	  std::tuple<Funcs...> m_funcs;

	public:
	  constexpr future_generator_split_t( ) noexcept
	    : m_funcs{} {}

	  template<typename... Functions,
	           std::enable_if_t<
	             (sizeof...( Functions ) > 1 &&
	              sizeof...( Functions ) == sizeof...( Funcs ) &&
	              !daw::is_same_v<std::tuple<Funcs...>,
	                              std::tuple<Functions...>>),
	             std::nullptr_t> = nullptr>
	  constexpr explicit future_generator_split_t( Functions &&... funcs )
	    : m_funcs( std::forward<Functions>( funcs )... ) {}

	  constexpr explicit future_generator_split_t(
	    std::tuple<ator_t<Funcs>...> const &tp_funcs )
	    : m_funcs( tp_funcs ) {}

	  constexpr explicit future_generator_split_t(
	    std::tuple<future_generator_t<Funcs>...> &&tp_funcs )
	    : m_funcs( daw::move( tp_funcs ) ) {}
	};
	*/
} // namespace daw

