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
#include <utility>

#include <daw/cpp_17.h>
#include <daw/daw_expected.h>
#include <daw/daw_semaphore.h>
#include <daw/daw_traits.h>

#include "task_scheduler.h"

namespace daw {
	enum class future_status { ready, timeout, deferred, continued };

	template<typename Result>
	struct future_result_t;

	template<>
	struct future_result_t<void>;

	namespace impl {
		class member_data_base_t {
			daw::shared_semaphore m_semaphore;
			future_status m_status;

		  protected:
			task_scheduler m_task_scheduler;

			member_data_base_t( task_scheduler ts )
			    : m_status{future_status::deferred}, m_task_scheduler{std::move( ts )} {}

			member_data_base_t( daw::shared_semaphore semaphore, task_scheduler ts )
			    : m_semaphore{std::move( semaphore )}
			    , m_status{future_status::deferred}
			    , m_task_scheduler{std::move( ts )} {}

		  public:
			member_data_base_t( ) = delete;
			member_data_base_t( member_data_base_t const & ) = delete;
			member_data_base_t( member_data_base_t && ) noexcept = delete;
			member_data_base_t &operator=( member_data_base_t const & ) = delete;
			member_data_base_t &operator=( member_data_base_t && ) noexcept = delete;
			virtual ~member_data_base_t( );

			void wait( ) {
				if( m_status != future_status::ready ) {
					m_semaphore.wait( );
				}
			}

			template<typename Rep, typename Period>
			future_status wait_for( std::chrono::duration<Rep, Period> const &rel_time ) {
				if( future_status::deferred == m_status || future_status::ready == m_status ) {
					return m_status;
				}
				if( m_semaphore.wait_for( rel_time ) ) {
					return m_status;
				}
				return future_status::timeout;
			}

			template<typename Clock, typename Duration>
			future_status wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) {
				if( future_status::deferred == m_status || future_status::ready == m_status ) {
					return m_status;
				}
				if( m_semaphore.wait_until( timeout_time ) ) {
					return m_status;
				}
				return future_status::timeout;
			}

			bool try_wait( ) {
				return m_semaphore.try_wait( );
			}

			void notify( ) {
				m_semaphore.notify( );
			}

			future_status &status( ) noexcept {
				return m_status;
			}

			virtual void set_exception( ) noexcept = 0;
			virtual void set_exception( std::exception_ptr ptr ) noexcept = 0;
		};

		template<typename base_result_t>
		struct member_data_t : public member_data_base_t {
			using expected_result_t = daw::expected_t<base_result_t>;
			using next_function_t = std::function<void( expected_result_t )>;
			next_function_t m_next;
			expected_result_t m_result;

			member_data_t( task_scheduler ts ) : member_data_base_t{std::move( ts )}, m_next{nullptr}, m_result{} {}

			explicit member_data_t( daw::shared_semaphore semaphore, task_scheduler ts )
			    : member_data_base_t{std::move( semaphore ), std::move( ts )}, m_next{nullptr}, m_result{} {}

			~member_data_t( ) override = default;

		  private:
			member_data_t( member_data_t const & ) = delete;
			member_data_t &operator=( member_data_t const & ) = delete;
			member_data_t( member_data_t &&other ) noexcept = delete;
			member_data_t &operator=( member_data_t &&rhs ) noexcept = delete;

			void pass_next( expected_result_t value ) noexcept {
				daw::exception::daw_throw_on_false( m_next, "Attempt to call next function on empty function" );
				m_next( std::move( value ) );
			}

		  public:
			void set_value( expected_result_t value ) noexcept {
				m_result = std::move( value );
				if( m_next ) {
					pass_next( std::move( m_result ) );
					return;
				}
				status( ) = future_status::ready;
				notify( );
			}

			void set_value( base_result_t value ) noexcept {
				set_value( expected_result_t{ value } );
			}

			void set_exception( std::exception_ptr ptr ) noexcept override {
				set_value( expected_result_t{ptr} );
			}

			void set_exception( ) noexcept override {
				set_exception( std::current_exception( ) );
			}

			template<typename Function, typename... Args>
			void from_code( Function func, Args &&... args ) {
				try {
					auto result = expected_from_code<base_result_t>( func, std::forward<Args>( args )... );
					set_value( std::move( result ) );
				} catch( ... ) { set_exception( std::current_exception( ) ); }
			}

			template<typename Function>
			auto next( Function next_func ) {
				daw::exception::daw_throw_on_true( m_next, "Can only set next function once" );
				using next_result_t = std::decay_t<decltype( next_func( std::declval<expected_result_t>( ).get( ) ) )>;

				future_result_t<next_result_t> result{m_task_scheduler};
				std::function<next_result_t( base_result_t )> func = next_func;
				auto ts = m_task_scheduler;
				m_next = [result, func=std::move(func), ts=std::move(ts)]( expected_result_t e_result ) mutable {
					if( e_result.has_exception( ) ) {
						result.set_exception( e_result.get_exception_ptr( ) );
						return;
					}
					ts.add_task( convert_function_to_task( result, func, e_result.get( ) ) );
				};

				if( future_status::ready == status( ) ) {
					pass_next( std::move( m_result ) );
				}
				status( ) = future_status::continued;
				notify( );
				return result;
			}
		};
		// member_data_t

		template<>
		struct member_data_t<void> : public member_data_base_t {
			using base_result_t = void;
			using expected_result_t = daw::expected_t<void>;
			using next_function_t = std::function<void( expected_result_t )>;
			next_function_t m_next;
			expected_result_t m_result;

			member_data_t( task_scheduler ts ) : member_data_base_t{std::move( ts )}, m_next{nullptr}, m_result{} {}

			explicit member_data_t( daw::shared_semaphore semaphore, task_scheduler ts )
			    : member_data_base_t{std::move( semaphore ), std::move( ts )}, m_next{nullptr}, m_result{} {}

			~member_data_t( ) override;

		  private:
			member_data_t( member_data_t const & ) = delete;
			member_data_t &operator=( member_data_t const & ) = delete;
			member_data_t( member_data_t &&other ) noexcept = delete;
			member_data_t &operator=( member_data_t &&rhs ) noexcept = delete;

			void pass_next( expected_result_t value ) noexcept {
				daw::exception::daw_throw_on_false( m_next, "Attempt to call next function on empty function" );
				m_next( std::move( value ) );
			}

		  public:
			void set_value( expected_result_t result ) noexcept;
			void set_value( ) noexcept;

			void set_exception( std::exception_ptr ptr ) noexcept override;

			void set_exception( ) noexcept override;

			template<typename Function, typename... Args>
			void from_code( Function func, Args &&... args ) {
				try {
					func( std::forward<Args>( args )... );
					set_value( );
				} catch( ... ) { set_exception( std::current_exception( ) ); }
			}

			template<typename Function>
			auto next( Function next_func ) {
				daw::exception::daw_throw_on_true( m_next, "Can only set next function once" );
				using next_result_t = std::decay_t<decltype( next_func( ) )>;
				future_result_t<next_result_t> result{m_task_scheduler};

				auto ts = m_task_scheduler;
				std::function<next_result_t( )> func = next_func;
				m_next = [result, func=std::move(func), ts=std::move(ts)]( expected_result_t e_result ) mutable {
					if( e_result.has_exception( ) ) {
						result.set_exception( e_result.get_exception_ptr( ) );
						return;
					}
					ts.add_task( convert_function_to_task( result, func ) );
				};

				if( future_status::ready == status( ) ) {
					pass_next( std::move( m_result ) );
				}
				status( ) = future_status::continued;
				notify( );
				return result;
			}
		};
		// member_data_t<void, daw::expected_t<void>>

		struct future_result_base_t {
			future_result_base_t( ) = default;
			future_result_base_t( future_result_base_t const & ) = default;
			future_result_base_t( future_result_base_t && ) noexcept = default;
			future_result_base_t &operator=( future_result_base_t const & ) = default;
			future_result_base_t &operator=( future_result_base_t && ) noexcept = default;

			virtual ~future_result_base_t( );
			virtual void wait( ) const = 0;
			virtual bool try_wait( ) const = 0;
			virtual explicit operator bool( ) const;
		}; // future_result_base_t

		template<typename... Unknown>
		struct function_to_task_t;

		template<typename Result, typename Function, typename Arg, typename... Args>
		struct function_to_task_t<Result, Function, Arg, Args...> {
			Result m_result;
			Function m_function;
			std::tuple<Arg, Args...> m_args;
			function_to_task_t( Result result, Function func, Arg &&arg, Args &&... args )
			    : m_result{std::move(result)}
			    , m_function{std::move( func )}
			    , m_args{std::forward<Arg>( arg ), std::forward<Args>( args )...} {}

			void operator( )( ) {
				m_result.from_code( [&]( ) { return daw::apply( m_function, m_args ); } );
			}
		}; // function_to_task_t

		template<typename Result, typename Function>
		struct function_to_task_t<Result, Function> {
			Result m_result;
			Function m_function;
			function_to_task_t( Result result, Function func )
			    : m_result{std::move(result)}
			    , m_function{std::move( func )} { }

			void operator( )( ) {
				m_result.from_code( m_function );
			}
		}; // function_to_task_t

		template<typename Result, typename Function, typename... Args>
		auto convert_function_to_task( Result &result, Function func, Args &&... args ) {
			return function_to_task_t<Result, Function, Args...>{result, std::move( func ),
			                                                     std::forward<Args>( args )...};
		}

		template<size_t N, size_t SZ, typename... Callables>
		struct call_func_t {
			template<typename Results, typename... Args>
			void operator( )( daw::task_scheduler &ts, daw::shared_semaphore semaphore, Results &results,
			                  std::tuple<Callables...> const &callables, std::tuple<Args...> const &args ) {
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
		class future_group_result_t {
			std::tuple<Functions...> tp_functions;

		  public:
			future_group_result_t( Functions... fs ) : tp_functions{std::make_tuple( std::move( fs )... )} {}

			template<typename... Args>
			auto operator( )( Args... args ) {
				using result_tp_t =
				    std::tuple<daw::expected_t<std::decay_t<decltype( std::declval<Functions>( )( args... ) )>>...>;

				auto tp_args = std::make_tuple( std::move( args )... );
				future_result_t<result_tp_t> result;
				daw::shared_semaphore semaphore{1 - static_cast<intmax_t>( sizeof...( Functions ) )};
				auto th_worker = [
					result, semaphore, tp_functions = std::move( tp_functions ), tp_args = std::move( tp_args )
				]( ) mutable noexcept {
					auto ts = get_task_scheduler( );
					result_tp_t tp_result;
					impl::call_funcs( ts, semaphore, tp_result, tp_functions, tp_args );

					semaphore.wait( );

					result.set_value( std::move( tp_result ) );
				};
				std::thread th{th_worker};
				th.detach( );
				return result;
			}
		}; // future_group_result_t
	}      // namespace impl
} // namespace daw
