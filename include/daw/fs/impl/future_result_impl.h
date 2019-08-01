// The MIT License (MIT)
//
// Copyright (c) 2016-2019 Darrell Wright
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
#include <daw/daw_move.h>
#include <daw/daw_traits.h>
#include <daw/daw_tuple_helper.h>
#include <daw/parallel/daw_latch.h>

#include "../task_scheduler.h"

namespace daw {
	enum class future_status { ready, timeout, deferred, continued };

	template<typename Result>
	struct future_result_t;

	template<>
	struct future_result_t<void>;

	namespace impl {
		class member_data_base_t {
			mutable daw::shared_latch m_semaphore;
			std::atomic<future_status> m_status;

		protected:
			task_scheduler m_task_scheduler;
			explicit member_data_base_t( task_scheduler ts );
			member_data_base_t( daw::shared_latch sem, task_scheduler ts );

		public:
			member_data_base_t( ) = delete;
			member_data_base_t( member_data_base_t const & ) = delete;
			member_data_base_t( member_data_base_t && ) = delete;
			member_data_base_t &operator=( member_data_base_t const & ) = delete;
			member_data_base_t &operator=( member_data_base_t && ) = delete;
			virtual ~member_data_base_t( ) noexcept;

			void wait( ) const;

			template<typename Rep, typename Period>
			future_status wait_for( std::chrono::duration<Rep, Period> rel_time ) {
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
			future_status
			wait_until( std::chrono::time_point<Clock, Duration> timeout_time ) {
				if( future_status::deferred == m_status ||
				    future_status::ready == m_status ) {
					return m_status;
				}
				if( m_semaphore.wait_until( timeout_time ) ) {
					return m_status;
				}
				return future_status::timeout;
			}

			bool try_wait( );
			void notify( );
			void status( future_status s );
			future_status status( ) const;
			virtual void set_exception( ) = 0;
			virtual void set_exception( std::exception_ptr ptr ) = 0;
		};

		template<size_t N, typename... Functions, typename... Results, typename Arg>
		[[nodiscard]] auto
		add_fork_task_impl( daw::shared_latch &sem, task_scheduler &ts,
		                    std::tuple<Results...> &results,
		                    std::tuple<Functions...> &funcs, Arg &&arg )
		  -> std::enable_if_t<( N == sizeof...( Functions ) - 1 ), bool> {

			daw::exception::DebugAssert( ts, "ts should never be null" );
			return daw::schedule_task(
			  sem,
			  [result = daw::mutable_capture( std::get<N>( results ) ),
			   func = daw::mutable_capture( std::get<N>( funcs ) ),
			   arg = daw::mutable_capture( std::forward<Arg>( arg ) )]( ) {
				  result->from_code( daw::move( *func ), daw::move( *arg ) );
			  },
			  ts );
		}

		template<size_t N, typename... Functions, typename... Results, typename Arg>
		[[nodiscard]] auto
		add_fork_task_impl( daw::shared_latch &sem, task_scheduler &ts,
		                    std::tuple<Results...> &results,
		                    std::tuple<Functions...> &funcs, Arg &&arg )
		  -> std::enable_if_t<( N < sizeof...( Functions ) - 1 ), bool> {

			daw::exception::DebugAssert( ts, "ts should never be null" );
			if( not daw::schedule_task(
			      sem,
			      [result = daw::mutable_capture( std::get<N>( results ) ),
			       func = daw::mutable_capture( std::get<N>( funcs ) ),
			       arg = daw::mutable_capture( arg )]( ) {
				      result->from_code( daw::move( *func ), daw::move( *arg ) );
			      },
			      ts ) ) {
				return false;
			}

			return add_fork_task_impl<N + 1>( sem, ts, results, funcs,
			                                  std::forward<Arg>( arg ) );
		} // namespace impl

		template<typename... Functions, typename... Results, typename Arg>
		[[nodiscard]] daw::shared_latch
		add_fork_task( task_scheduler &ts, std::tuple<Results...> &results,
		               std::tuple<Functions...> &funcs, Arg &&arg ) {

			auto sem = daw::shared_latch( sizeof...( Functions ) );
			if( not add_fork_task_impl<0>( sem, ts, results, funcs,
			                               std::forward<Arg>( arg ) ) ) {

				throw ::daw::unable_to_add_task_exception{};
			}
			return sem;
		}

		template<typename Result>
		struct member_data_t : public member_data_base_t,
		                       std::enable_shared_from_this<member_data_t<Result>> {
			using base_result_t = Result;
			using expected_result_t = daw::expected_t<base_result_t>;
			using next_function_t = std::function<void( expected_result_t )>;
			next_function_t m_next;
			expected_result_t m_result;

			explicit member_data_t( task_scheduler ts )
			  : member_data_base_t( daw::move( ts ) )
			  , m_next( nullptr )
			  , m_result( ) {}

			member_data_t( daw::shared_latch sem, task_scheduler ts )
			  : member_data_base_t( daw::move( sem ), daw::move( ts ) )
			  , m_next( nullptr )
			  , m_result( ) {}

			~member_data_t( ) override = default;

			member_data_t( member_data_t const & ) = delete;
			member_data_t &operator=( member_data_t const & ) = delete;
			member_data_t( member_data_t && ) = delete;
			member_data_t &operator=( member_data_t && ) = delete;

		private:
			[[nodiscard]] decltype( auto ) pass_next( expected_result_t &&value ) {
				daw::exception::daw_throw_on_false(
				  m_next, "Attempt to call next function on empty function" );

				return m_next( daw::move( value ) );
			}

			[[nodiscard]] decltype( auto )
			pass_next( expected_result_t const &value ) {
				daw::exception::daw_throw_on_false(
				  m_next, "Attempt to call next function on empty function" );

				return m_next( value );
			}

		public:
			void set_value( expected_result_t &&value ) {
				if( static_cast<bool>( m_next ) ) {
					pass_next( daw::move( value ) );
					return;
				}
				m_result = daw::move( value );
				status( future_status::ready );
				notify( );
			}

			void set_value( expected_result_t const &value ) {
				auto const has_next = static_cast<bool>( m_next );
				if( has_next ) {
					pass_next( value );
					return;
				}
				m_result = value;
				status( future_status::ready );
				notify( );
			}

			void set_value( base_result_t &&value ) {
				auto const has_next = static_cast<bool>( m_next );
				if( has_next ) {
					pass_next( expected_result_t( daw::move( value ) ) );
					return;
				}
				m_result = daw::move( value );
				status( future_status::ready );
				notify( );
			}

			void set_value( base_result_t const &value ) {
				auto const has_next = static_cast<bool>( m_next );
				if( has_next ) {
					pass_next( expected_result_t{value} );
					return;
				}
				m_result = value;
				status( future_status::ready );
				notify( );
			}

			void set_exception( std::exception_ptr ptr ) override {
				auto const has_next = static_cast<bool>( m_next );
				if( has_next ) {
					pass_next( expected_result_t{ptr} );
					return;
				}
				m_result = ptr;
				status( future_status::ready );
				notify( );
			}

			[[nodiscard]] bool is_exception( ) const {
				wait( );
				// TODO: look into not throwing and allowing values to be retrieved
				daw::exception::daw_throw_on_true(
				  status( ) == future_status::continued,
				  "Attempt to use a future that has been continued" );
				return m_result.has_exception( );
			}

			[[nodiscard]] decltype( auto ) get( ) {
				wait( );
				daw::exception::daw_throw_on_true(
				  status( ) == future_status::continued,
				  "Attempt to use a future that has been continued" );
				return m_result.get( );
			}

			[[nodiscard]] decltype( auto ) get( ) const {
				wait( );
				daw::exception::daw_throw_on_true(
				  status( ) == future_status::continued,
				  "Attempt to use a future that has been continued" );
				return m_result.get( );
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
			template<
			  typename Function,
			  std::enable_if_t<!std::is_function_v<std::remove_reference_t<Function>>,
			                   std::nullptr_t> = nullptr>
			[[nodiscard]] auto next( Function &&func ) {
				daw::exception::daw_throw_on_true( m_next,
				                                   "Can only set next function once" );
				using next_result_t = std::invoke_result_t<Function, base_result_t>;

				auto result = future_result_t<next_result_t>( m_task_scheduler );

				m_next = [result = daw::mutable_capture( result ),
				          func = daw::mutable_capture( std::forward<Function>( func ) ),
				          ts = daw::mutable_capture( m_task_scheduler ),
				          self = this->shared_from_this( )](
				           expected_result_t value ) -> void {
					if( value.has_value( ) ) {
						if( not ts->add_task(
						      [result = daw::mutable_capture( std::move( *result ) ),
						       func = daw::mutable_capture( daw::move( *func ) ),
						       v = daw::mutable_capture( daw::move( value.get( ) ) )]( ) {
							      result->from_code( daw::move( *func ), daw::move( *v ) );
						      } ) ) {

							throw ::daw::unable_to_add_task_exception{};
						}
					} else {
						result->set_exception( value.get_exception_ptr( ) );
					}
				};
				if( future_status::ready == status( ) ) {
					pass_next( daw::move( m_result ) );
					status( future_status::continued );
				} else {
					status( future_status::continued );
					notify( );
				}
				return result;
			}

			template<typename... Functions>
			[[nodiscard]] auto fork( Functions &&... funcs ) {
				using result_t =
				  std::tuple<future_result_t<daw::remove_cvref_t<decltype(
				    funcs( std::declval<expected_result_t>( ).get( ) ) )>>...>;

				auto const construct_future = [&]( auto &&f ) {
					Unused( f );
					using fut_t = future_result_t<decltype(
					  f( std::declval<expected_result_t>( ).get( ) ) )>;

					return fut_t( m_task_scheduler );
				};
				auto result = result_t( construct_future( funcs )... );
				m_next = [result = mutable_capture( result ),
				          tpfuncs = daw::mutable_capture(
				            std::tuple<daw::remove_cvref_t<Functions>...>(
				              std::forward<Functions>( funcs )... ) ),
				          ts = daw::mutable_capture( m_task_scheduler ),
				          self = this->shared_from_this( )]( auto &&value )
				  -> std::enable_if_t<daw::is_same_v<
				    expected_result_t, daw::remove_cvref_t<decltype( value )>>> {
					if( value.has_value( ) ) {
						if( not ts->add_task( impl::add_fork_task( *ts, *result, *tpfuncs,
						                                           value.get( ) ) ) ) {

							throw ::daw::unable_to_add_task_exception{};
						}
					} else {
						daw::tuple::apply( *result,
						                   [ptr = value.get_exception_ptr( )]( auto &t ) {
							                   t.set_exception( ptr );
						                   } );
					}
				};
				status( future_status::continued );
				if( future_status::ready == status( ) ) {
					pass_next( daw::move( m_result ) );
				} else {
					notify( );
				}
				return result;
			}
		}; // namespace impl
		// member_data_t

		template<>
		struct member_data_t<void>
		  : public member_data_base_t,
		    std::enable_shared_from_this<member_data_t<void>> {
			using base_result_t = void;
			using expected_result_t = daw::expected_t<void>;
			using next_function_t = std::function<void( expected_result_t )>;
			next_function_t m_next;
			expected_result_t m_result;

			explicit member_data_t( task_scheduler ts )
			  : member_data_base_t( daw::move( ts ) )
			  , m_next( nullptr )
			  , m_result( ) {}

			explicit member_data_t( daw::shared_latch sem, task_scheduler ts )
			  : member_data_base_t( daw::move( sem ), daw::move( ts ) )
			  , m_next( nullptr )
			  , m_result( ) {}

			~member_data_t( ) override;

			member_data_t( member_data_t const & ) = delete;
			member_data_t &operator=( member_data_t const & ) = delete;
			member_data_t( member_data_t && ) = delete;
			member_data_t &operator=( member_data_t && ) = delete;

		private:
			void pass_next( expected_result_t &&value ) {
				daw::exception::daw_throw_on_false(
				  m_next, "Attempt to call next function on empty function" );
				daw::exception::dbg_throw_on_true(
				  value.has_exception( ), "Unexpected exception in expected_t" );

				m_next( daw::move( value ) );
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
				static_assert( traits::is_callable_v<Function, Args...>,
				               "Cannot call func with args provided" );
				try {
					func( std::forward<Args>( args )... );
					set_value( );
				} catch( ... ) { set_exception( std::current_exception( ) ); }
			}

			template<typename Function>
			[[nodiscard]] auto next( Function &&func ) {
				daw::exception::daw_throw_on_true( m_next,
				                                   "Can only set next function once" );
				using next_result_t =
				  daw::traits::invoke_result_t<std::remove_reference_t<Function>>;

				auto result = future_result_t<next_result_t>( m_task_scheduler );

				m_next = [result = daw::mutable_capture( result ),
				          func = daw::mutable_capture( std::forward<Function>( func ) ),
				          ts = daw::mutable_capture( m_task_scheduler ),
				          self = this->shared_from_this( )](
				           expected_result_t value ) -> void {
					if( value.has_value( ) ) {
						if( ts->add_task(
						      [result = daw::mutable_capture( daw::move( *result ) ),
						       func = daw::mutable_capture( daw::move( *func ) )]( ) {
							      result->from_code( daw::move( *func ) );
						      } ) ) {

							throw ::daw::unable_to_add_task_exception{};
						}
					} else {
						result->set_exception( value.get_exception_ptr( ) );
					}
				};
				if( future_status::ready == status( ) ) {
					pass_next( daw::move( m_result ) );
					status( future_status::continued );
				} else {
					status( future_status::continued );
					notify( );
				}
				return result;
			}

			template<typename... Functions>
			[[nodiscard]] auto fork( Functions &&... funcs ) {
				using result_t = std::tuple<
				  future_result_t<daw::remove_cvref_t<decltype( funcs( ) )>>...>;

				auto const construct_future = [&]( auto &&f ) {
					Unused( f );
					using fut_t = future_result_t<decltype( f( ) )>;
					return fut_t( m_task_scheduler );
				};
				auto result = result_t{construct_future( funcs )...};

				auto tpfuncs = std::tuple<daw::remove_cvref_t<Functions>...>(
				  std::forward<Functions>( funcs )... );
				m_next = [result, tpfuncs = daw::move( tpfuncs ), ts = m_task_scheduler,
				          self = this->shared_from_this( )]( auto &&value ) mutable
				  -> std::enable_if_t<daw::is_same_v<
				    expected_result_t, daw::remove_cvref_t<decltype( value )>>> {
					if( value.has_value( ) ) {
						ts.add_task( impl::add_fork_task( ts, result, tpfuncs ) );
					} else {
						daw::tuple::apply( result,
						                   [ptr = value.get_exeption_ptr( )]( auto &&t ) {
							                   t.set_exception( ptr );
						                   } );
					}
				};
				if( future_status::ready == status( ) ) {
					pass_next( daw::move( m_result ) );
					status( future_status::continued );
				} else {
					status( future_status::continued );
					notify( );
				}
				return result;
			}

			template<typename Function, typename... Functions>
			[[nodiscard]] auto fork_join( Function &&next_func,
			                              Functions &&... funcs ) {
				using result_t = std::tuple<
				  future_result_t<daw::remove_cvref_t<decltype( funcs( ) )>>...>;

				auto const construct_future = [&]( auto &&f ) {
					Unused( f );
					using fut_t = future_result_t<decltype( f( ) )>;
					return fut_t( m_task_scheduler );
				};
				auto result = result_t{construct_future( funcs )...};

				auto tpfuncs = std::tuple<daw::remove_cvref_t<Functions>...>(
				  std::forward<Functions>( funcs )... );
				m_next = [result, tpfuncs = daw::move( tpfuncs ), ts = m_task_scheduler,
				          self = this->shared_from_this( )]( auto &&value ) mutable
				  -> std::enable_if_t<daw::is_same_v<
				    expected_result_t, daw::remove_cvref_t<decltype( value )>>> {
					if( value.has_value( ) ) {
						ts.add_task( impl::add_fork_task( ts, result, tpfuncs ) );
					} else {
						daw::tuple::apply(
						  result, [ptr = value.get_exception_ptr( )]( auto &&t ) {
							  std::forward<decltype( t )>( t ).set_exception( ptr );
						  } );
					}
				};
				if( future_status::ready == status( ) ) {
					pass_next( daw::move( m_result ) );
					status( future_status::continued );
				} else {
					status( future_status::continued );
					notify( );
				}
				return result;
			}
		};
		// member_data_t<void, daw::expected_t<void>>

		struct future_result_base_t {
			future_result_base_t( ) noexcept = default;
			future_result_base_t( future_result_base_t const & ) noexcept = default;
			future_result_base_t( future_result_base_t && ) noexcept = default;
			future_result_base_t &
			operator=( future_result_base_t const & ) noexcept = default;
			future_result_base_t &
			operator=( future_result_base_t && ) noexcept = default;

			virtual ~future_result_base_t( ) noexcept;
			virtual void wait( ) const = 0;
			[[nodiscard]] virtual bool try_wait( ) const = 0;
		}; // future_result_base_t

		template<size_t N, size_t SZ, typename... Callables>
		struct apply_many_t {
			template<typename... ResultTypes, typename... Args>
			void operator( )( daw::task_scheduler &ts, daw::shared_latch sem,
			                  std::tuple<ResultTypes...> &results,
			                  std::tuple<Callables...> const &callables,
			                  std::shared_ptr<std::tuple<Args...>> const &tp_args ) {

				// TODO this looks weird
				if( not schedule_task(
				      sem,
				      [results = daw::mutable_capture( results ),
				       callables = daw::mutable_capture( std::cref( callables ) ),
				       tp_args = mutable_capture( std::cref( tp_args ) )]( ) {
					      try {
						      std::get<N>( *results ) = std::apply(
						        std::get<N>( callables->get( ) ), *( tp_args->get( ) ) );
					      } catch( ... ) { std::get<N>( *results ).set_exception( ); }
				      },
				      ts ) ) {

					throw ::daw::unable_to_add_task_exception{};
				}

				apply_many_t<N + 1, SZ, Callables...>{}( ts, sem, results, callables,
				                                         tp_args );
			}
		}; // namespace impl

		template<size_t SZ, typename... Functions>
		struct apply_many_t<SZ, SZ, Functions...> {
			template<typename Results, typename... Args>
			constexpr void
			operator( )( daw::task_scheduler const &, daw::shared_latch const &,
			             Results const &, std::tuple<Functions...> const &,
			             std::shared_ptr<std::tuple<Args...>> const & ) {}
		}; // apply_many_t<SZ, SZ, Functions..>

		template<typename Result, typename... Functions, typename... Args>
		void apply_many( daw::task_scheduler &ts, daw::shared_latch sem,
		                 Result &result, std::tuple<Functions...> const &callables,
		                 std::shared_ptr<std::tuple<Args...>> tp_args ) {

			apply_many_t<0, sizeof...( Functions ), Functions...>{}(
			  ts, sem, result, callables, tp_args );
		}

		template<typename... Functions>
		class future_group_result_t {
			std::tuple<Functions...> tp_functions;

		public:
			template<
			  typename... Fs,
			  daw::enable_if_t<
			    ( sizeof...( Fs ) != 1 or
			      !std::is_same_v<
			        future_group_result_t,
			        daw::remove_cvref_t<daw::traits::first_type<Fs...>>> )> = nullptr>
			explicit constexpr future_group_result_t( Fs &&... fs )
			  : tp_functions( std::forward<Fs>( fs )... ) {}

			template<typename... Args>
			[[nodiscard]] auto operator( )( Args &&... args ) {
				using result_tp_t =
				  std::tuple<daw::expected_t<daw::remove_cvref_t<decltype(
				    std::declval<Functions>( )( std::forward<Args>( args )... ) )>>...>;

				// Copy arguments to const, non-ref, non-volatile versions in a
				// shared_pointer so that only one copy is ever created
				auto tp_args = std::make_shared<
				  std::tuple<std::add_const_t<daw::remove_cvref_t<Args>>...>>(
				  std::forward<Args>( args )... );

				auto ts = get_task_scheduler( );
				auto sem = daw::shared_latch( sizeof...( Functions ) );
				auto result = future_result_t<result_tp_t>( sem, ts );

				auto th_worker =
				  [result = daw::mutable_capture( result ),
				   sem = daw::mutable_capture( sem ),
				   tp_functions = daw::mutable_capture( daw::move( tp_functions ) ),
				   ts = daw::mutable_capture( ts ),
				   tp_args = daw::mutable_capture( daw::move( tp_args ) )]( ) {
					  auto const oe = ::daw::on_scope_exit( [&sem]( ) { sem->notify( ); } );

					  auto tp_result = result_tp_t( );
					  impl::apply_many( *ts, *sem, tp_result, daw::move( *tp_functions ),
					                    daw::move( *tp_args ) );

					  sem->wait( );
					  result->set_value( daw::move( tp_result ) );
				  };
				try {
					if( not ts.add_task( daw::move( th_worker ) ) ) {
						throw ::daw::unable_to_add_task_exception{};
					}
				} catch( std::system_error const &e ) {
					std::cerr << "Error creating thread, aborting.\n Error code: "
					          << e.code( ) << "\nMessage: " << e.what( ) << std::endl;
					std::abort( );
				}
				return result;
			}
		};
	} // namespace impl
} // namespace daw
