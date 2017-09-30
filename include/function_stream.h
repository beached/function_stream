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

#include "function_stream_impl.h"

namespace daw {
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

		constexpr function_stream( std::tuple<Functions...> tpfuncs )
		    : m_funcs{std::move( tpfuncs )}, continue_on_result_destruction{true} {}

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
	void wait_for_function_streams( FunctionStream &function_stream ) {
		function_stream.wait( );
	}

	template<typename... FunctionStreams>
	void wait_for_function_streams( FunctionStreams &&... function_streams ) {
		[]( ... ) {}( ( wait_for_function_streams( function_streams ), 0 )... );
	}

	template<typename T, typename... Ts>
	std::vector<T> create_vector( T &&value, Ts &&... values ) {
		return std::vector<T>{std::initializer_list<T>{std::forward<T>( value ), std::forward<Ts>( values )...}};
	}

	template<typename... Funcs>
	class future_generator_t {
		std::tuple<Funcs...> m_funcs;

		template<typename... Functions>
		static auto make_future_generator( std::tuple<Functions...> tp_funcs ) {
			return future_generator_t<Functions...>{std::move( tp_funcs )};
		}

	  public:
		future_generator_t( Funcs... funcs ) noexcept : m_funcs{std::move( funcs )...} {}
		future_generator_t( std::tuple<Funcs...> tp_funcs ) : m_funcs{std::move( tp_funcs )} {}

		template<typename... Args>
		auto operator( )( Args &&... args ) const {
			return get_function_stream( )( std::forward<Args...>( args )... );
		}

		function_stream<Funcs...> get_function_stream( ) const {
			return function_stream<Funcs...>( m_funcs );
		}

		template<typename... NextFunctions>
		auto next( NextFunctions... next_functions ) const {
			auto tp_next_funcs = std::tuple_cat( m_funcs, std::make_tuple( std::move( next_functions )... ) );
			return make_future_generator( tp_next_funcs );
		}
	};

	template<typename... Functions>
	auto make_future_generator( Functions... functions ) {
		return future_generator_t<Functions...>{std::move( functions )...};
	}

	template<typename NextFunction, typename... Functions>
	auto operator>>( future_generator_t<Functions...> const &lhs, NextFunction next_func ) {
		return lhs.next( std::move( next_func ) );
	}
} // namespace daw

