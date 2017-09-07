// The MIT License (MIT)
//
// Copyright (c) 2016-2017 Darrell Wright
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

#include <daw/cpp_17.h>
#include <daw/daw_expected.h>
#include <daw/daw_semaphore.h>
#include <daw/daw_traits.h>

#include "task_scheduler.h"

namespace daw {
	enum class future_status { ready, timeout, deferred };

	template<typename Result>
	struct future_result_t;

	template<>
	struct future_result_t<void>;


	namespace impl {
		struct future_result_base_t {
			future_result_base_t( ) = default;
			future_result_base_t( future_result_base_t const & ) = default;
			future_result_base_t( future_result_base_t && ) noexcept = default;
			future_result_base_t &operator=( future_result_base_t const & ) = default;
			future_result_base_t &operator=( future_result_base_t && ) noexcept = default;

			virtual ~future_result_base_t( );
			virtual void wait( ) const = 0;
			virtual bool try_wait( ) const = 0;
			explicit operator bool( ) const;
		}; // future_result_base_t

		template<typename Result, typename Function, typename... Args>
		struct f_caller_t {
			Result &m_result;
			Function m_function;
			std::tuple<Args...> m_args;
			f_caller_t( Result &result, Function func, Args &&... args )
			    : m_result{result}, m_function{std::move( func )}, m_args{std::forward<Args>( args )...} {}

			void operator( )( ) {
				m_result.from_code( [&]( ) { return daw::apply( m_function, m_args ); } );
			}
		}; // f_caller_t

		template<typename Result, typename Function, typename... Args>
		auto make_f_caller( Result &result, Function func, Args &&... args ) {
			return f_caller_t<Result, Function, Args...>{result, std::move( func ), std::forward<Args>( args )...};
		}

		template<size_t N, size_t SZ, typename... Callables>
		struct call_func_t {
			template<typename Results, typename... Args>
			void operator( )( daw::task_scheduler &ts, daw::shared_semaphore semaphore, Results &results,
			                  std::tuple<Callables...> const &callables, std::tuple<Args...> const & args ) {
				schedule_task( semaphore, [&results, &callables, &args ]( ) mutable noexcept {
					try {
						std::get<N>( results ) = daw::apply( std::get<N>( callables ), args );
					} catch( ... ) { std::get<N>( results ) = std::current_exception; }
				},
				               ts );
				call_func_t<N + 1, SZ, Callables...>{}( ts, semaphore, results, callables, args );
			}
		}; // call_func_t

		template<size_t SZ, typename... Callables>
		struct call_func_t<SZ, SZ, Callables...> {
			template<typename Results, typename... Args>
			constexpr void operator( )( daw::task_scheduler const &, daw::shared_semaphore const &, Results const &,
			                            std::tuple<Callables...> const &, std::tuple<Args...> const & ) noexcept {}
		}; // call_func_t<SZ, SZ, Callables..>

		template<typename Result, typename... Callables, typename... Args>
		void call_funcs( daw::task_scheduler &ts, daw::shared_semaphore semaphore, Result &result,
		                 std::tuple<Callables...> const &callables, std::tuple<Args...> args ) {
			call_func_t<0, sizeof...( Callables ), Callables...>{}( ts, semaphore, result, callables, args );
		}
	
		template<typename... Functions>
		class result_t {
			std::tuple<Functions...> tp_functions;
		public:
			result_t( Functions... fs ): tp_functions{std::make_tuple( std::move( fs )... )} { }

			template<typename... Args>
			auto operator()( Args... args ) {
				daw::shared_semaphore semaphore{1 - static_cast<intmax_t>( sizeof...( Functions ) )};
				using result_t = std::tuple<daw::expected_t<std::decay_t<decltype( std::declval<Functions>( )( args... ) )>>...>;
				auto tp_args = std::make_tuple( std::move( args )... );
				future_result_t<result_t> result;
				auto th = std::thread{
					[result, semaphore, tp_functions = std::move( tp_functions ), tp_args=std::move(tp_args)]( ) mutable noexcept {
						auto ts = get_task_scheduler( );
						result_t tp_result;
						impl::call_funcs( ts, semaphore, tp_result, tp_functions, tp_args );

						ts.blocking_on_waitable( semaphore );

				        result.set_value( std::move( tp_result ) );
					}
				};
				th.detach( );
				return result;
			}
		};	// result_t
	} // namespace impl
} // namespace daw

