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
#include <mutex>
#include <utility>
#include <vector>

#include <daw/daw_expected.h>

namespace daw {
	template<typename Function>
	struct function_call_impl {
		template<typename... Args>
		void operator( )( Args&&... args ) {
			using result_t = decltype( Function{}( std::forward<Args>( args )... ) );
			std::pair<size_t, daw::expected<result_t>> result = { pos, {} };
			result.second = result.from_code( []( ) {
				return Function{}( std::forward<Args>( args )... );
			} );
			return result;
		}
	};

	template<typename Function>
	struct function_call {
		template<typename Callback, size_t pos, typename... Args>
		void operator( )( Args&&... args ) {
			using result_t = decltype( function_call_impl<Function>{}( std::forward<Args>( args )... ) );
			std::pair<size_t, result_t> result = { pos, {} };
			result.second = result.from_code( []( ) {
				return Function{}( std::forward<Args>( args )... );
			} );
			Callback{}( result );
		}
	};

	template<size_t pos, typename Function, typename... Functions>
	struct function_call {
		template<typename Callback, typename... Args>
		void operator( )( Args&&... args ) {
			auto tmp = function_call<Callback, pos, Function>( std::forward<Args>( args )... );
			function_call<Callback, pos + 1, Functions...>{}( std::move( tmp ) );
		}
	};

	template<typename... Functions>
	struct function_queue {
		static_assert( sizeof...( Functions ) > 0, "Must supply at least one function" );

		function_queue( ) = default;
		~function_queue( ) = default;
		function_queue( function_queue const & ) = default;
		function_queue( function_queue && ) = default;
		function_queue & operator=( function_queue const & ) = default;
		function_queue & operator=( function_queue && ) = default;

		/// Completion is called with an std::pair<size_t, daw::expected_t<result_type>> where the value,
		// if any is that of the last completed function and a counter
		template<typename Completion, typename... Args>
		void push( Completion cb, Args&&... args ) {
			using result_t = decltype( function_call<0, Functions...>{}( std::forward<Args>( args )... ) );

			std::pair<size_t, daw::expected_t<boost::any>>> result { 0, {} };
			
		}
	private:
		template<typename Result>
		Result trans( result_t result ) {
			if( !result.has_value( ) && !result.has_exception( ) ) {
				throw no_value_exception{ };
			}
			return boost::any_cast<Result>( result.get( ) );
		}
		
		template<typename Arg, typename Tuple>
		void set_value( boost::optional<Arg> & out_val, boost::any const & in_val ) {
			out_val = boost::any_cast<Arg>( in_val );
		}

		template<typename Arg, typename Tuple, std::size_t... I>
		auto p2t_impl( Tuple & t, parameter_t const & param, std::index_sequence<I...>) {
			set_value( std::get<I>( t ), *std::next( param.begin( ), I ) )...;
		}

		template<typename Tuple, typename Indices = std::make_index_sequence<sizeof...(Args)>>
		void p2t( Tuple & t, parameter_t const & param ) {
			return p2t_impl( t, param, Indices{ } );
		}
	

		template<typename TupleIn, std::size_t... I>
		auto to_base_tuple_impl( TupleIn const & in_tuple, std::index_sequence<I...> ) {
			return std::make_tuple( std::get<I>( in_tuple ).get( )... ); 
		}

		template<typename... Args>
		auto to_base_tuple( std::tuple<daw::expected_t<Args...> const & t ) {
			return to_base_tuple_impl( t, std::make_index_sequence<sizeof...(Args)>{ } ); 
		}
			
	public:
		template<typename... Args>
		auto convert_parameters_to( parameter_t const & param ) {
			auto opt_tuple = std::make_tuple( daw::expected_t<Args>{ }... );
			p2t( opt_tuple, param );	

		}

		template<typename Result>
		Result pop_result_as( ) {
			return trans<Result>( pop_result( ) );
		}

		template<typename Result>
		std::vector<Result> pop_results_as( ) {
			std::vector<Result> results;
			auto erased_results = pop_results( );
			std::transform( erased_results.begin( ), erased_results.end( ), std::back_inserter( results ), []( auto const & v ) { return trans<Result>( v ); } );
			return result;
		}

		void push_argument_list( parameter_t param );

		template<typename... Args>
		void push_arguments( Args const &... args ) {
			push_argument_list( { boost::any{ args }... } );
		}
	};	// function_queue

}    // namespace daw

