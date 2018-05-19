// The MIT License (MIT)
//
// Copyright (c) 2016-2018 Darrell Wright
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

#include <iostream>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include <daw/cpp_17.h>
#include <daw/daw_expected.h>
#include <daw/daw_semaphore.h>
#include <daw/daw_traits.h>
#include <daw/daw_utility.h>

#include "task_scheduler.h"

namespace daw {
	enum class future_status { ready, timeout, deferred, continued };

	template<typename Result>
	struct future_result_t;

	template<>
	struct future_result_t<void>;

	namespace impl {
		template<typename... Unknown>
		struct function_to_task_t;

		template<typename Result, typename Function, typename... Args>
		struct function_to_task_t<Result, Function, Args...>;

		template<typename Result, typename Function, typename... Args>
		constexpr function_to_task_t<Result, Function, Args...>
		convert_function_to_task( Result &&result, Function &&func,
		                          Args &&... args );

		class member_data_base_t {
			daw::shared_semaphore m_semaphore;
			future_status m_status;

		protected:
			task_scheduler m_task_scheduler;

			explicit member_data_base_t( task_scheduler ts )
			  : m_semaphore{}
			  , m_status{future_status::deferred}
			  , m_task_scheduler{std::move( ts )} {}

			member_data_base_t( daw::shared_semaphore sem, task_scheduler ts )
			  : m_semaphore{std::move( sem )}
			  , m_status{future_status::deferred}
			  , m_task_scheduler{std::move( ts )} {}

		public:
			member_data_base_t( ) = delete;
			member_data_base_t( member_data_base_t const & ) = delete;
			member_data_base_t( member_data_base_t && ) = delete;
			member_data_base_t &operator=( member_data_base_t const & ) = delete;
			member_data_base_t &operator=( member_data_base_t && ) = delete;
			virtual ~member_data_base_t( );

			void wait( ) {
				if( m_status != future_status::ready ) {
					m_semaphore.wait( );
				}
			}

			template<typename Rep, typename Period>
			future_status
			wait_for( std::chrono::duration<Rep, Period> const &rel_time ) {
				if( future_status::deferred == m_status ||
				    future_status::ready == m_status ) {
					return m_status;
				}
				if( m_semaphore.wait_for( rel_time ) ) {
					return m_status;
				}
				return future_status::timeout;
			}

			template<typename Clock, typename Duration>
			future_status wait_until(
			  std::chrono::time_point<Clock, Duration> const &timeout_time ) {
				if( future_status::deferred == m_status ||
				    future_status::ready == m_status ) {
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

			future_status &status( ) {
				return m_status;
			}

			virtual void set_exception( ) = 0;
			virtual void set_exception( std::exception_ptr ptr ) = 0;
		};

		template<size_t N, typename... Functions, typename... Results, typename Arg>
		auto add_split_task( task_scheduler &ts, std::tuple<Results...> &results,
		                     std::tuple<Functions...> &funcs, Arg &&arg )
		  -> std::enable_if_t<( N == sizeof...( Functions ) - 1 ), void> {

			ts.add_task( convert_function_to_task( std::get<N>( results ),
			                                       std::get<N>( funcs ),
			                                       std::forward<Arg>( arg ) ) );
		}

		template<size_t N, typename... Functions, typename... Results, typename Arg>
		auto add_split_task( task_scheduler &ts, std::tuple<Results...> &results,
		                     std::tuple<Functions...> &funcs, Arg &&arg )
		  -> std::enable_if_t<( N < sizeof...( Functions ) - 1 ), void> {

			ts.add_task( convert_function_to_task( std::get<N>( results ),
			                                       std::get<N>( funcs ), arg ) );

			add_split_task<N + 1>( ts, results, funcs, std::forward<Arg>( arg ) );
		}

		template<typename Result>
		struct member_data_t : public member_data_base_t {
			using base_result_t = Result;
			using expected_result_t = daw::expected_t<base_result_t>;
			using next_function_t = std::function<void( expected_result_t )>;
			next_function_t m_next;
			expected_result_t m_result;

			explicit member_data_t( task_scheduler ts )
			  : member_data_base_t{std::move( ts )}
			  , m_next{}
			  , m_result{} {}

			member_data_t( daw::shared_semaphore sem, task_scheduler ts )
			  : member_data_base_t{std::move( sem ), std::move( ts )}
			  , m_next{}
			  , m_result{} {}

			~member_data_t( ) override = default;

			member_data_t( member_data_t const & ) = delete;
			member_data_t &operator=( member_data_t const & ) = delete;
			member_data_t( member_data_t &&other ) = delete;
			member_data_t &operator=( member_data_t &&rhs ) = delete;

		private:
			decltype( auto ) pass_next( expected_result_t &&value ) {
				daw::exception::daw_throw_on_false(
				  m_next, "Attempt to call next function on empty function" );
				daw::exception::dbg_throw_on_true(
				  value.has_exception( ), "Unexpected exception in expected_t" );

				return m_next( std::move( value ) );
			}

			decltype( auto ) pass_next( expected_result_t const &value ) {
				daw::exception::daw_throw_on_false(
				  m_next, "Attempt to call next function on empty function" );
				daw::exception::dbg_throw_on_true(
				  value.has_exception( ), "Unexpected exception in expected_t" );

				return m_next( value );
			}

		public:
			void set_value( expected_result_t &&value ) {
				if( !value.has_exception( ) && m_next ) {
					pass_next( std::move( value ) );
					return;
				}
				m_result = std::move( value );
				status( ) = future_status::ready;
				notify( );
			}

			void set_value( expected_result_t const &value ) {
				if( !value.has_exception( ) && m_next ) {
					pass_next( value );
					return;
				}
				m_result = value;
				status( ) = future_status::ready;
				notify( );
			}

			void set_value( base_result_t &&value ) {
				if( m_next ) {
					pass_next( expected_result_t{std::move( value )} );
					return;
				}
				m_result = std::move( value );
				status( ) = future_status::ready;
				notify( );
			}

			void set_value( base_result_t const &value ) {
				if( m_next ) {
					pass_next( expected_result_t{value} );
					return;
				}
				m_result = value;
				status( ) = future_status::ready;
				notify( );
			}

			void set_exception( std::exception_ptr ptr ) override {
				set_value( expected_result_t{ptr} );
			}

			void set_exception( ) override {
				set_exception( std::current_exception( ) );
			}

			template<typename Function, typename... Args>
			void from_code( Function &&func, Args &&... args ) {
				try {
					set_value( expected_from_code( std::forward<Function>( func ),
					                               std::forward<Args>( args )... ) );
				} catch( ... ) { set_exception( std::current_exception( ) ); }
			}

		public:
			template<typename Function>
			auto next( Function &&next_func ) {
				daw::exception::daw_throw_on_true( m_next,
				                                   "Can only set next function once" );
				using next_result_t = std::decay_t<decltype(
				  next_func( std::declval<expected_result_t>( ).get( ) ) )>;

				auto result = future_result_t<next_result_t>{m_task_scheduler};

				m_next = [result, next_func = std::forward<Function>( next_func ),
				          ts = m_task_scheduler](
				           expected_result_t e_result ) mutable -> void {
					e_result.visit( daw::make_overload(
					  [&]( base_result_t &&value ) {
						  ts.add_task( convert_function_to_task(
						    std::forward<future_result_t<next_result_t>>( result ),
						    std::move( next_func ), std::move( value ) ) );
					  },
					  [&]( base_result_t const &value ) {
						  ts.add_task( convert_function_to_task(
						    std::forward<future_result_t<next_result_t>>( result ),
						    std::move( next_func ), value ) );
					  },
					  []( daw::empty_value_t ) {
						  std::terminate( ); // This can never happen and if it does we
						                     // cannot reason about the state
					  },
					  [&]( std::exception_ptr ptr ) { result.set_exception( ptr ); } ) );
				};

				if( future_status::ready == status( ) ) {
					pass_next( std::move( m_result ) );
				}
				status( ) = future_status::continued;
				notify( );
				return result;
			}

			template<typename... Functions>
			auto split( Functions &&... funcs ) {
				using result_t = std::tuple<future_result_t<decltype(
				  funcs( std::declval<expected_result_t>( ).get( ) ) )>...>;

				result_t result{m_task_scheduler};
				auto tpfuncs = std::make_tuple( std::forward<Functions>( funcs )... );

				m_next = [ts = m_task_scheduler, result = std::move( result ),
				          tpfuncs = std::move( tpfuncs )](
				           expected_result_t e_result ) mutable -> void {
					e_result.visit( daw::make_overload(
					  [&]( base_result_t &&value ) {
						  impl::add_split_task<0>( ts, result, tpfuncs, e_result.get( ) );
					  },
					  [&]( base_result_t const &value ) {
						  impl::add_split_task<0>( ts, result, tpfuncs, e_result.get( ) );
					  },
					  []( daw::empty_value_t ) {
						  std::terminate( ); // This can never happen and if it does we
						                     // cannot reason about the state
					  },
					  [&]( std::exception_ptr ptr ) { result.set_exception( ptr ); } ) );
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

			explicit member_data_t( task_scheduler ts )
			  : member_data_base_t{std::move( ts )}
			  , m_next{nullptr}
			  , m_result{} {}

			explicit member_data_t( daw::shared_semaphore sem, task_scheduler ts )
			  : member_data_base_t{std::move( sem ), std::move( ts )}
			  , m_next{nullptr}
			  , m_result{} {}

			~member_data_t( ) override;

			member_data_t( member_data_t const & ) = delete;
			member_data_t &operator=( member_data_t const & ) = delete;
			member_data_t( member_data_t &&other ) = delete;
			member_data_t &operator=( member_data_t &&rhs ) = delete;

		private:
			void pass_next( expected_result_t &&value ) {
				daw::exception::daw_throw_on_false(
				  m_next, "Attempt to call next function on empty function" );
				daw::exception::dbg_throw_on_true(
				  value.has_exception( ), "Unexpected exception in expected_t" );

				m_next( value );
			}

			void pass_next( expected_result_t const &value ) {
				daw::exception::daw_throw_on_false(
				  m_next, "Attempt to call next function on empty function" );
				daw::exception::dbg_throw_on_true(
				  value.has_exception( ), "Unexpected exception in expected_t" );

				m_next( value );
			}

		public:
			void set_value( expected_result_t result );
			void set_value( );

			void set_exception( std::exception_ptr ptr ) override;

			void set_exception( ) override;

			template<typename Function, typename... Args>
			void from_code( Function &&func, Args &&... args ) {
				try {
					func( std::forward<Args>( args )... );
					set_value( );
				} catch( ... ) { set_exception( std::current_exception( ) ); }
			}

			template<typename Function>
			auto next( Function &&next_func ) {
				daw::exception::daw_throw_on_true( m_next,
				                                   "Can only set next function once" );
				using next_result_t = std::decay_t<decltype( next_func( ) )>;
				auto result = future_result_t<next_result_t>{m_task_scheduler};
				auto func =
				  std::function<next_result_t( )>{std::forward<Function>( next_func )};

				m_next = [result, func = std::move( func ), ts = m_task_scheduler](
				           expected_result_t e_result ) mutable -> void {
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
			template<typename... Functions>
			auto split( Functions &&... funcs ) {
				using result_t = std::tuple<future_result_t<decltype( funcs( ) )>...>;

				auto result = result_t{m_task_scheduler};
				auto tpfuncs = std::make_tuple( std::forward<Functions>( funcs )... );

				m_next = [ts = m_task_scheduler, result = std::move( result ),
				          tpfuncs = std::move( tpfuncs )](
				           expected_result_t e_result ) mutable -> void {
					if( e_result.has_exception( ) ) {
						result.set_exception( e_result.get_exception_ptr( ) );
						return;
					}
					impl::add_split_task<0>( ts, result, tpfuncs, e_result.get( ) );
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
			future_result_base_t( future_result_base_t && ) = default;
			future_result_base_t &operator=( future_result_base_t const & ) = default;
			future_result_base_t &operator=( future_result_base_t && ) = default;

			virtual ~future_result_base_t( );
			virtual void wait( ) const = 0;
			virtual bool try_wait( ) const = 0;
		}; // future_result_base_t

		template<typename... Unknown>
		struct function_to_task_t;

		template<typename Result, typename Function, typename... Args>
		struct function_to_task_t<Result, Function, Args...> {
			Result m_result;
			Function m_function;
			std::tuple<Args...> m_args;

			template<typename R, typename F, typename... A>
			constexpr function_to_task_t( R &&result, F &&func, A &&... args )
			  : m_result{std::forward<R>( result )}
			  , m_function{std::forward<F>( func )}
			  , m_args{std::forward<A>( args )...} {}

			constexpr void operator( )( ) {
				m_result.from_code(
				  [&]( ) { return daw::apply( m_function, m_args ); } );
			}
		}; // function_to_task_t

		template<typename Result, typename Function, typename... Args>
		using detect_has_from_code = decltype( std::declval<Result>( ).from_code(
		  std::declval<Function>( ), std::declval<Args>( )... ) );

		template<typename Result, typename Function, typename... Args>
		constexpr function_to_task_t<Result, Function, Args...>
		convert_function_to_task( Result &&result, Function &&func,
		                          Args &&... args ) {

			static_assert(
			  daw::is_detected_v<detect_has_from_code, Result, Function, Args...>,
			  "Result must have a from_code member that takes a callable" );

			return function_to_task_t<Result, Function, Args...>(
			  std::forward<Result>( result ), std::forward<Function>( func ),
			  std::forward<Args>( args )... );
		}

		template<size_t N, size_t SZ, typename... Callables>
		struct apply_many_t {
			template<typename Results, typename... Args>
			void operator( )( daw::task_scheduler &ts, daw::shared_semaphore sem,
			                  Results &results,
			                  std::tuple<Callables...> const &callables,
			                  std::shared_ptr<std::tuple<Args...>> const &tp_args ) {

				schedule_task( sem,
				               [&results, &callables, tp_args]( ) mutable {
					               try {
						               std::get<N>( results ) =
						                 daw::apply( std::get<N>( callables ), *tp_args );
					               } catch( ... ) {
						               std::get<N>( results ) = std::current_exception;
					               }
				               },
				               ts );

				apply_many_t<N + 1, SZ, Callables...>{}( ts, sem, results, callables,
				                                         tp_args );
			}
		}; // apply_many_t

		template<size_t SZ, typename... Functions>
		struct apply_many_t<SZ, SZ, Functions...> {
			template<typename Results, typename... Args>
			constexpr void
			operator( )( daw::task_scheduler const &, daw::shared_semaphore const &,
			             Results const &, std::tuple<Functions...> const &,
			             std::shared_ptr<std::tuple<Args...>> const & ) {}
		}; // apply_many_t<SZ, SZ, Functions..>

		template<typename Result, typename... Functions, typename... Args>
		void apply_many( daw::task_scheduler &ts, daw::shared_semaphore sem,
		                 Result &result, std::tuple<Functions...> const &callables,
		                 std::shared_ptr<std::tuple<Args...>> tp_args ) {

			apply_many_t<0, sizeof...( Functions ), Functions...>{}(
			  ts, sem, result, callables, tp_args );
		}

		template<typename... Functions>
		class future_group_result_t {
			std::tuple<std::decay_t<Functions>...> tp_functions;

		public:
			constexpr explicit future_group_result_t( Functions &&... fs )
			  : tp_functions{std::make_tuple( std::forward<Functions>( fs )... )} {}

			template<typename... Args>
			auto operator( )( Args &&... args ) {
				using result_tp_t = std::tuple<daw::expected_t<std::decay_t<decltype(
				  std::declval<Functions>( )( std::forward<Args>( args )... ) )>>...>;

				// Copy arguments to const, non-ref, non-volatile versions in a
				// shared_pointer so that only one copy is ever created
				auto tp_args =
				  std::make_shared<std::tuple<std::add_const_t<std::decay_t<Args>>...>>(
				    std::make_tuple( std::forward<Args>( args )... ) );

				auto result = future_result_t<result_tp_t>{};

				auto sem = daw::shared_semaphore{
				  1 - static_cast<intmax_t>( sizeof...( Functions ) )};

				auto th_worker = [result, sem, tp_functions = std::move( tp_functions ),
				                  tp_args = std::move( tp_args )]( ) mutable {
					auto ts = get_task_scheduler( );
					result_tp_t tp_result;
					impl::apply_many( ts, sem, tp_result, tp_functions,
					                  std::move( tp_args ) );

					sem.wait( );

					result.set_value( std::move( tp_result ) );
				};
				try {
					std::thread{th_worker}.detach( );
				} catch( std::system_error const &e ) {
					std::cerr << "Error creating thread, aborting.\n Error code: "
					          << e.code( ) << "\nMessage: " << e.what( ) << std::endl;
					std::terminate( );
				}
				return result;
			}
		}; // future_group_result_t
	}    // namespace impl
} // namespace daw
