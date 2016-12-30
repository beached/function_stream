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
#include <functional>
#include <mutex>
#include <utility>

#include <daw/daw_expected.h>

namespace daw {
	struct modification_while_running_exception: public std::exception {
		modification_while_running_exception( );
		~modification_while_running_exception( );
		modification_while_running_exception( modification_while_running_exception const & ) = default;
		modification_while_running_exception( modification_while_running_exception && ) = default;
		modification_while_running_exception & operator=( modification_while_running_exception const & ) = default;
		modification_while_running_exception & operator=( modification_while_running_exception && ) = default;
	};	// modification_while_running_exception

	struct no_value_exception: public std::exception {
		no_value_exception( );
		~no_value_exception( );
		no_value_exception( no_value_exception const & ) = default;
		no_value_exception( no_value_exception && ) = default;
		no_value_exception & operator=( no_value_exception const & ) = default;
		no_value_exception & operator=( no_value_exception && ) = default;
	};	// no_value_exception

	struct function_queue {
		using parameter_t = std::initializer_list<boost::any>;
		using result_t = daw::Expected<parameter_t>;
		using function_t = std::function<result_t( parameter_t );
		private:
		std::vector<function_t> m_functions;
		std::vector<result_t> m_results;
		mutable std::mutex m_result_lock;
		public:
		function_queue( ) = default;
		~function_queue( ) = default;
		function_queue( function_queue const & ) = default;
		function_queue( function_queue && ) = default;
		function_queue & operator=( function_queue const & ) = default;
		function_queue & operator=( function_queue && ) = default;

		std::vector<function_t> & function_queue( );
		std::vector<function_t> const & function_queue( ) const;

		bool has_results( ) const noexcept;
		result_t pop_result( ) noexcept;
		std::vector<result_t> pop_results( ) noexcept;

		private:
		template<typename Result>
			Result trans( result_t result ) {
				if( !result.has_value( ) && !result.has_exception( ) ) {
					throw no_value{ };
				}
				return boost::any_cast<Result>( result.get( ) );
			}

		template<typename Tuple, std::size_t... I>
		decltype(auto) p2t_impl( Tuple &  parameter_t param, std::index_sequence<I...>) {
			return std::make_tuple( boost::any_cast<Args>( *std::next( param.begin( ), I ) ) );
		}

		template<typename... Args, typename Indices = std::make_index_sequence<sizeof...(Args)>>
		decltype(auto) p2t( parameter_t param ) {
			return p2t_impl( param, Indices{ } );
		}
		public:
		template<typename... Args>
			auto convert_parameters_to( parameter_t const & param ) {
				std::tuple<Args...> result;
				return std::make_tuple<Args...>( boost::any_cast<Args>( 

							return result;
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

