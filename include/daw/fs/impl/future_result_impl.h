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
	enum class future_status : uint8_t { ready, timeout, deferred, continued };

	template<typename Result>
	struct [[nodiscard]] future_result_t;

	template<>
	struct future_result_t<void>;

	namespace impl {
		template<typename expected_result_t, typename next_function_t>
		struct [[nodiscard]] member_data_members {
			using next_t =
			  ::daw::lockable_value_t<next_function_t, std::recursive_mutex>;

			task_scheduler m_task_scheduler;
			next_t m_next = next_t( next_function_t( ) );
			daw::shared_latch m_semaphore = daw::shared_latch( );
			std::atomic<future_status> m_status = future_status::deferred;

			expected_result_t m_result = expected_result_t( );

			explicit member_data_members( task_scheduler ts )
			  : m_task_scheduler( std::move( ts ) ) {}

			member_data_members( daw::shared_latch sem, task_scheduler ts )
			  : m_task_scheduler( std::move( ts ) )
			  , m_semaphore( std::move( sem ) ) {}

			template<typename Rep, typename Period>
			[[nodiscard]] future_status wait_for(
			  std::chrono::duration<Rep, Period> rel_time ) {
				if( future_status::deferred == m_status or
				    future_status::ready == m_status ) {
					return m_status;
				}
				if( m_semaphore.wait_for( rel_time ) ) {
					return m_status;
				}
				return future_status::timeout;
			}

			template<typename Clock, typename Duration>
			[[nodiscard]] future_status wait_until(
			  std::chrono::time_point<Clock, Duration> timeout_time ) {
				if( future_status::deferred == m_status or
				    future_status::ready == m_status ) {
					return m_status;
				}
				if( m_semaphore.wait_until( timeout_time ) ) {
					return m_status;
				}
				return future_status::timeout;
			}

			void wait( ) const {
				if( m_status != future_status::ready ) {
					m_semaphore.wait( );
				}
			}

			[[nodiscard]] bool try_wait( ) const {
				return m_semaphore.try_wait( );
			}

			void notify( ) {
				m_semaphore.notify( );
			}

			void status( future_status new_status ) {
				m_status = new_status;
			}

			future_status status( ) const {
				return static_cast<future_status>( m_status );
			}
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
		struct [[nodiscard]] member_data_t {
			using base_result_t = Result;
			using expected_result_t = daw::expected_t<base_result_t>;
			using next_function_t = std::function<void( expected_result_t )>;

			using data_t =
			  impl::member_data_members<expected_result_t, next_function_t>;

			std::shared_ptr<data_t> m_data;

			explicit member_data_t( task_scheduler ts )
			  : m_data( std::make_shared<data_t>( daw::move( ts ) ) ) {}

			member_data_t( daw::shared_latch sem, task_scheduler ts )
			  : m_data(
			      std::make_shared<data_t>( daw::move( sem ), daw::move( ts ) ) ) {}

		private:
			explicit member_data_t( std::shared_ptr<data_t> && dptr ) noexcept
			  : m_data( daw::move( dptr ) ) {}

			[[nodiscard]] decltype( auto ) pass_next( expected_result_t && value ) {
				auto nxt = m_data->m_next.get( );
				::daw::exception::precondition_check(
				  *nxt, "Attempt to call next function on empty function" );

				return ( *nxt )( std::move( value ) );
			}

			[[nodiscard]] decltype( auto ) pass_next(
			  expected_result_t const &value ) {

				auto nxt = m_data->m_next.get( );
				::daw::exception::precondition_check(
				  *nxt, "Attempt to call next function on empty function" );

				return ( *nxt )( value );
			}

		public:
			void set_value( expected_result_t && value ) {
				if( auto nxt = m_data->m_next.get( ); *nxt ) {
					pass_next( daw::move( value ) );
					return;
				}
				m_data->m_result = daw::move( value );
				m_data->status( future_status::ready );
				m_data->notify( );
			} // namespace impl

			void set_value( expected_result_t const &value ) {
				if( auto nxt = m_data->m_next.get( ); *nxt ) {
					pass_next( value );
					return;
				}
				m_data->m_result = value;
				m_data->status( future_status::ready );
				m_data->notify( );
			}

			void set_value( base_result_t && value ) {
				if( auto nxt = m_data->m_next.get( ); *nxt ) {
					pass_next(
					  ::daw::construct_a<expected_result_t>( ::daw::move( value ) ) );
					return;
				}
				m_data->m_result = daw::move( value );
				m_data->status( future_status::ready );
				m_data->notify( );
			}

			void set_value( base_result_t const &value ) {
				if( auto nxt = m_data->m_next.get( ); *nxt ) {
					pass_next( ::daw::construct_a<expected_result_t>( value ) );
					return;
				}
				m_data->m_result = value;
				m_data->status( future_status::ready );
				m_data->notify( );
			}

			void set_exception( std::exception_ptr ptr ) {
				if( auto nxt = m_data->m_next.get( ); *nxt ) {
					pass_next( ::daw::construct_a<expected_result_t>( ptr ) );
					return;
				}
				m_data->m_result = ptr;
				m_data->status( future_status::ready );
				m_data->notify( );
			}

			[[nodiscard]] bool is_exception( ) const {
				m_data->wait( );
				// TODO: look into not throwing and allowing values to be retrieved
				daw::exception::daw_throw_on_true(
				  m_data->status( ) == future_status::continued,
				  "Attempt to use a future that has been continued" );
				return m_data->m_result.has_exception( );
			}

			[[nodiscard]] decltype( auto ) get( ) {
				m_data->wait( );
				daw::exception::daw_throw_on_true(
				  m_data->status( ) == future_status::continued,
				  "Attempt to use a future that has been continued" );
				return m_data->m_result.get( );
			}

			[[nodiscard]] decltype( auto ) get( ) const {
				m_data->wait( );
				daw::exception::daw_throw_on_true(
				  m_data->status( ) == future_status::continued,
				  "Attempt to use a future that has been continued" );
				return m_data->m_result.get( );
			}

			void set_exception( ) {
				set_exception( std::current_exception( ) );
			}

			template<typename Function, typename... Args>
			void from_code( Function && func, Args && ... args ) {
				try {
					set_value( expected_from_code( std::forward<Function>( func ),
					                               std::forward<Args>( args )... ) );
				} catch( ... ) { set_exception( std::current_exception( ) ); }
			}

			template<
			  typename Function,
			  std::enable_if_t<!std::is_function_v<std::remove_reference_t<Function>>,
			                   std::nullptr_t> = nullptr>
			[[nodiscard]] auto next( Function && func ) {
				assert( m_data );
				auto nxt = m_data->m_next.get( );
				assert( !*nxt ); // can only set next function once

				using next_result_t = decltype( func( std::declval<base_result_t>( ) ) );

				auto result =
				  future_result_t<next_result_t>( m_data->m_task_scheduler );

				*nxt = [result = daw::mutable_capture( result ),
				        func = daw::mutable_capture( std::forward<Function>( func ) ),
				        ts = daw::mutable_capture( m_data->m_task_scheduler ),
				        self = *this]( expected_result_t value ) -> void {
					if( not value.has_value( ) ) {
						result->set_exception( value.get_exception_ptr( ) );
						return;
					}
					if( not ts->add_task(
					      [result = daw::mutable_capture( std::move( *result ) ),
					       func = daw::mutable_capture( daw::move( *func ) ),
					       v = daw::mutable_capture( daw::move( value.get( ) ) )]( ) {
						      result->from_code( daw::move( *func ), daw::move( *v ) );
					      } ) ) {

						throw ::daw::unable_to_add_task_exception{};
					}
				};
				if( future_status::ready == m_data->status( ) ) {
					pass_next( daw::move( m_data->m_result ) );
					m_data->status( future_status::continued );
				} else {
					m_data->status( future_status::continued );
					nxt.release( );
					m_data->notify( );
				}
				return result;
			}

			template<typename... Functions>
			[[nodiscard]] auto fork( Functions && ... funcs ) {
				assert( m_data );
				auto nxt = m_data->m_next.get( );
				assert( !*nxt ); // can only set next function once

				using result_t =
				  std::tuple<future_result_t<daw::remove_cvref_t<decltype(
				    funcs( std::declval<expected_result_t>( ).get( ) ) )>>...>;

				auto const construct_future = [&]( auto &&f ) {
					Unused( f );
					using fut_t = future_result_t<decltype(
					  f( std::declval<expected_result_t>( ).get( ) ) )>;

					return fut_t( m_data->m_task_scheduler );
				};
				auto result = result_t( construct_future( funcs )... );
				*nxt = [result = mutable_capture( result ),
				        tpfuncs = daw::mutable_capture(
				          std::tuple<daw::remove_cvref_t<Functions>...>(
				            std::forward<Functions>( funcs )... ) ),
				        ts = daw::mutable_capture( m_data->m_task_scheduler ),
				        self = *this]( auto &&value )
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
				m_data->status( future_status::continued );
				if( future_status::ready == m_data->status( ) ) {
					pass_next( daw::move( m_data->m_result ) );
				} else {
					nxt.release( );
					m_data->notify( );
				}
				return result;
			}

			template<typename Function, typename... Functions>
			[[nodiscard]] auto fork_join( Function && joiner,
			                              Functions && ... funcs ) {

				// TODO: finish implementing
				Unused( joiner );
				Unused( funcs... );
				assert( m_data );
				auto nxt = m_data->m_next.get( );
				assert( *nxt ); // can only set next function once

				static_assert( (
				  std::is_invocable_v<Functions,
				                      decltype(
				                        std::declval<expected_result_t>( ).get( ) )> and
				  ... ) );

				/* TODO Finish
				using result_temp_t =
				  std::tuple<future_result_t<daw::remove_cvref_t<decltype(
				    funcs( std::declval<expected_result_t>( ).get( ) ) )>>...>;
				*/
				// using result_final_t =
				// not implemented
				std::abort( );
			}

			void wait( ) const {
				m_data->wait( );
			}

			[[nodiscard]] bool try_wait( ) const {
				return m_data->try_wait( );
			}

			[[nodiscard]] auto get_handle( ) const {
				class handle_t {
					std::weak_ptr<data_t> m_handle;

					explicit handle_t( std::weak_ptr<data_t> wptr )
					  : m_handle( wptr ) {}

					friend member_data_t;

				public:
					bool expired( ) const {
						return m_handle.expired( );
					}

					std::optional<member_data_t> lock( ) const {
						if( auto lck = m_handle.lock( ); lck ) {
							return member_data_t( std::move( lck ) );
						}
						return {};
					}
				};

				return handle_t( static_cast<std::weak_ptr<data_t>>( m_data ) );
			}
		}; // namespace impl

		template<>
		struct [[nodiscard]] member_data_t<void> {

			using base_result_t = void;
			using expected_result_t = daw::expected_t<void>;
			using next_function_t = std::function<void( expected_result_t )>;

			using data_t =
			  impl::member_data_members<expected_result_t, next_function_t>;

			std::shared_ptr<data_t> m_data;

			explicit member_data_t( task_scheduler ts )
			  : m_data( std::make_shared<data_t>( daw::move( ts ) ) ) {}

			explicit member_data_t( daw::shared_latch sem, task_scheduler ts )
			  : m_data(
			      std::make_shared<data_t>( daw::move( sem ), daw::move( ts ) ) ) {}

		private:
			explicit member_data_t( std::shared_ptr<data_t> && dptr ) noexcept
			  : m_data( daw::move( dptr ) ) {}

			void pass_next( expected_result_t && value ) {
				auto nxt = m_data->m_next.get( );
				daw::exception::precondition_check(
				  *nxt, "Attempt to call next function on empty function" );

				daw::exception::precondition_check(
				  not value.has_exception( ), "Unexpected exception in expected_t" );

				( *nxt )( daw::move( value ) );
			}

			void pass_next( expected_result_t const &value ) {
				auto nxt = m_data->m_next.get( );
				daw::exception::precondition_check(
				  *nxt, "Attempt to call next function on empty function" );

				daw::exception::precondition_check(
				  not value.has_exception( ), "Unexpected exception in expected_t" );

				( *nxt )( value );
			}

		public:
			void set_value( expected_result_t result );
			void set_value( );
			void set_exception( std::exception_ptr ptr );
			void set_exception( );

			template<typename Function, typename... Args>
			void from_code( Function && func, Args && ... args ) {
				static_assert( traits::is_callable_v<Function, Args...>,
				               "Cannot call func with args provided" );
				try {
					func( std::forward<Args>( args )... );
					set_value( );
				} catch( ... ) { set_exception( std::current_exception( ) ); }
			}

			template<typename Function>
			[[nodiscard]] auto next( Function && func ) {
				auto nxt = m_data->m_next.get( );
				daw::exception::precondition_check( not *nxt,
				                                    "Can only set next function once" );
				using next_result_t = decltype( std::declval<std::remove_reference_t<Function>>( )( ) );

				auto result =
				  future_result_t<next_result_t>( m_data->m_task_scheduler );

				*nxt = [result = daw::mutable_capture( result ),
				        func = daw::mutable_capture( std::forward<Function>( func ) ),
				        ts = daw::mutable_capture( m_data->m_task_scheduler ),
				        self = *this]( expected_result_t value ) -> void {
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
				if( future_status::ready == m_data->status( ) ) {
					pass_next( daw::move( m_data->m_result ) );
					m_data->status( future_status::continued );
				} else {
					m_data->status( future_status::continued );
					nxt.release( );
					m_data->notify( );
				}
				return result;
			}

			template<typename... Functions>
			[[nodiscard]] auto fork( Functions && ... funcs ) {
				auto nxt = m_data->m_next.get( );
				daw::exception::precondition_check( !*nxt,
				                                    "Can only set next function once" );
				using result_t = std::tuple<
				  future_result_t<daw::remove_cvref_t<decltype( funcs( ) )>>...>;

				auto const construct_future = [&]( auto &&f ) {
					Unused( f );
					using fut_t = future_result_t<decltype( f( ) )>;
					return fut_t( m_data->m_task_scheduler );
				};

				auto result =
				  ::daw::construct_a<result_t>( construct_future( funcs )... );

				auto tpfuncs = std::tuple<daw::remove_cvref_t<Functions>...>(
				  std::forward<Functions>( funcs )... );
				*nxt = [result, tpfuncs = daw::move( tpfuncs ),
				        ts = m_data->m_task_scheduler,
				        self = *this]( auto &&value ) mutable
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
				if( future_status::ready == m_data->status( ) ) {
					pass_next( daw::move( m_data->m_result ) );
					m_data->status( future_status::continued );
				} else {
					m_data->status( future_status::continued );
					nxt.release( );
					m_data->notify( );
				}
				return result;
			}

			template<typename Function, typename... Functions>
			[[nodiscard]] auto fork_join( Function && joiner,
			                              Functions && ... funcs ) {
				// TODO: finish implementing
				Unused( joiner );
				static_assert( ( std::is_invocable_v<Functions> and ... ) );
				auto nxt = m_data->m_next.get( );
				daw::exception::precondition_check( !*nxt,
				                                    "Can only set next function once" );

				auto const construct_future = [&]( auto &&f ) {
					// Default constructs a future of the result type with the task
					// scheduler
					Unused( f );
					return future_result_t<daw::remove_cvref_t<decltype( f( ) )>>(
					  m_data->m_task_scheduler );
				};

				// Create a place to put results of functions
				auto result = std::tuple( construct_future( funcs )... );

				auto tpfuncs = std::tuple( std::forward<Functions>( funcs )... );

				*nxt = [result = ::daw::mutable_capture( result ),
				        tpfuncs = ::daw::mutable_capture( daw::move( tpfuncs ) ),
				        ts = ::daw::mutable_capture( m_data->m_task_scheduler ),
				        self = *this]( auto const &value ) {
					static_assert(
					  daw::is_same_v<expected_result_t,
					                 daw::remove_cvref_t<decltype( value )>> );
					if( value.has_value( ) ) {
						ts->add_task( impl::add_fork_task( ts, result, tpfuncs ) );
						return;
					}
					daw::tuple::apply(
					  result, [ptr = value.get_exception_ptr( )]( auto &&t ) {
						  std::forward<decltype( t )>( t ).set_exception( ptr );
					  } );
				};
				if( future_status::ready == m_data->status( ) ) {
					pass_next( daw::move( m_data->m_result ) );
					m_data->status( future_status::continued );
				} else {
					m_data->status( future_status::continued );
					nxt.release( );
					m_data->notify( );
				}
				return result;
			}

			inline void wait( ) const {
				m_data->wait( );
			}

			[[nodiscard]] inline bool try_wait( ) const {
				return m_data->try_wait( );
			}

			[[nodiscard]] bool is_exception( ) const {
				m_data->wait( );
				// TODO: look into not throwing and allowing values to be retrieved
				daw::exception::daw_throw_on_true(
				  m_data->status( ) == future_status::continued,
				  "Attempt to use a future that has been continued" );
				return m_data->m_result.has_exception( );
			}

			void get( ) const {
				m_data->wait( );
				daw::exception::daw_throw_on_true(
				  m_data->status( ) == future_status::continued,
				  "Attempt to use a future that has been continued" );
				m_data->m_result.get( );
			}

			[[nodiscard]] auto get_handle( ) const {
				class handle_t {
					std::weak_ptr<data_t> m_handle;

					explicit handle_t( std::weak_ptr<data_t> wptr )
					  : m_handle( wptr ) {}

					friend member_data_t;

				public:
					bool expired( ) const {
						return m_handle.expired( );
					}

					std::optional<member_data_t> lock( ) const {
						if( auto lck = m_handle.lock( ); lck ) {
							return member_data_t( std::move( lck ) );
						}
						return {};
					}
				};

				return handle_t( static_cast<std::weak_ptr<data_t>>( m_data ) );
			}
		};

		struct [[nodiscard]] future_result_base_t {
			future_result_base_t( ) noexcept = default;
			future_result_base_t( future_result_base_t const & ) noexcept = default;
			future_result_base_t( future_result_base_t && ) noexcept = default;
			future_result_base_t &operator=( future_result_base_t const & ) noexcept =
			  default;
			future_result_base_t &operator=( future_result_base_t && ) noexcept =
			  default;

			virtual ~future_result_base_t( ) noexcept;
			virtual void wait( ) const = 0;
			[[nodiscard]] virtual bool try_wait( ) const = 0;
		}; // future_result_base_t

		template<size_t N, size_t SZ, typename... Callables>
		struct [[nodiscard]] apply_many_t{
		  template<typename... ResultTypes, typename... Args> void operator( )(
		    daw::task_scheduler &ts, daw::shared_latch sem,
		    std::tuple<ResultTypes...> &results,
		    std::tuple<Callables...> const &callables,
		    std::shared_ptr<std::tuple<Args...>> const &tp_args ){

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
		          ts ) ){

		      throw ::daw::unable_to_add_task_exception{};
	} // namespace impl

	apply_many_t<N + 1, SZ, Callables...>{}( ts, sem, results, callables,
	                                         tp_args );
} // namespace daw
}
;

template<size_t SZ, typename... Functions>
struct [[nodiscard]] apply_many_t<SZ, SZ, Functions...>{
  template<typename Results, typename... Args> constexpr void operator( )(
    daw::task_scheduler const &, daw::shared_latch const &, Results const &,
    std::tuple<Functions...> const &,
    std::shared_ptr<std::tuple<Args...>> const
      & ){}}; // apply_many_t<SZ, SZ, Functions..>

template<typename Result, typename... Functions, typename... Args>
void apply_many( daw::task_scheduler &ts, daw::shared_latch sem, Result &result,
                 std::tuple<Functions...> const &callables,
                 std::shared_ptr<std::tuple<Args...>> tp_args ) {

	apply_many_t<0, sizeof...( Functions ), Functions...>{}( ts, sem, result,
	                                                         callables, tp_args );
}

template<typename... Functions>
class [[nodiscard]] future_group_result_t {
	std::tuple<Functions...> tp_functions;

public:
	template<
	  typename... Fs,
	  daw::enable_if_t<(
	    sizeof...( Fs ) != 1 or
	    !std::is_same_v<future_group_result_t,
	                    daw::remove_cvref_t<daw::traits::first_type<Fs...>>> )> =
	    nullptr>
	explicit constexpr future_group_result_t( Fs && ... fs )
	  : tp_functions( std::forward<Fs>( fs )... ) {}

	template<typename... Args>
	[[nodiscard]] auto operator( )( Args &&... args ) {
		using result_tp_t = std::tuple<daw::expected_t<daw::remove_cvref_t<decltype(
		  std::declval<Functions>( )( std::forward<Args>( args )... ) )>>...>;

		// Copy arguments to const, non-ref, non-volatile versions in a
		// shared_pointer so that only one copy is ever created
		auto tp_args = std::make_shared<
		  std::tuple<std::add_const_t<daw::remove_cvref_t<Args>>...>>(
		  std::forward<Args>( args )... );

		auto ts = get_task_scheduler( );
		auto sem = daw::shared_latch( sizeof...( Functions ) );
		auto result = future_result_t<result_tp_t>( sem, ts );

		auto th_worker = [result = daw::mutable_capture( result ),
		                  sem = daw::mutable_capture( sem ),
		                  tp_functions = daw::mutable_capture( tp_functions ),
		                  ts = daw::mutable_capture( ts ),
		                  tp_args =
		                    daw::mutable_capture( daw::move( tp_args ) )]( ) {
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
