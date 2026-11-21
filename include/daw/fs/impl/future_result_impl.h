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

#include "../task_scheduler.h"
#include "daw_latch.h"

#include <daw/cpp_17.h>
#include <daw/daw_expected.h>
#include <daw/daw_move.h>
#include <daw/daw_mutable_capture.h>
#include <daw/daw_traits.h>
#include <daw/daw_tuple_helper.h>

#include <iostream>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

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
			  daw::lockable_value_t<next_function_t, std::recursive_mutex>;

			task_scheduler m_task_scheduler;
			next_t m_next = next_t( next_function_t( ) );
			daw::shared_cnt_sem m_semaphore = daw::shared_cnt_sem( 1 );
			std::atomic<future_status> m_status = future_status::deferred;

			expected_result_t m_result = expected_result_t( );

			explicit member_data_members( task_scheduler ts )
			  : m_task_scheduler( std::move( ts ) ) {}

			member_data_members( daw::shared_cnt_sem sem, task_scheduler ts )
			  : m_task_scheduler( std::move( ts ) )
			  , m_semaphore( std::move( sem ) ) {}

			template<typename Rep, typename Period>
			[[nodiscard]] future_status
			wait_for( std::chrono::duration<Rep, Period> rel_time ) {
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
			[[nodiscard]] future_status
			wait_until( std::chrono::time_point<Clock, Duration> timeout_time ) {
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

		template<typename... Functions, typename... Results, typename Arg>
		[[nodiscard]] daw::shared_cnt_sem
		add_fork_task( task_scheduler &ts, std::tuple<Results...> &results,
		               std::tuple<Functions...> &funcs, Arg &&arg ) {

			auto sem = daw::shared_cnt_sem( 1 );
			auto const ae = on_scope_exit( [sem]( ) mutable {
				sem.notify( );
			} );
			auto const fork_task =
			  [&]<std::size_t N>( std::integral_constant<std::size_t, N> ) {
				  return ts.add_task(
				    [r = daw::mutable_capture( std::get<N>( results ) ),
						 func = daw::mutable_capture( std::get<N>( funcs ) ),
						 ar = daw::mutable_capture( arg )]( ) //
				    {
					    r->from_code( func.move_out( ), ar.move_out( ) );
				    },
				    sem );
			  };
			auto const fork_tasks =
			  [&]<std::size_t... Is>( std::index_sequence<Is...> ) {
				  return ( fork_task( std::integral_constant<std::size_t, Is>{ } ) and
					         ... );
			  };
			if( not fork_tasks(
			      std::make_index_sequence<sizeof...( Functions )>{ } ) ) {
				throw daw::unable_to_add_task_exception{ };
			}
			return sem;
		}

		/// Shared state for future_result_t::fork_join.  Every fork function is
		/// run as its own task on the scheduler.  Each task stores its result in
		/// its slot and then decrements remaining.  The task that brings it to
		/// zero calls the joiner with all of the results.  No worker blocks
		/// waiting on the others.
		/// ArgsTuple is std::tuple<Value> for a non-void source future and
		/// std::tuple<> for a void one.
		template<typename Joiner, typename ArgsTuple, typename... Functions>
		struct [[nodiscard]] fork_join_state_t {
			static_assert( sizeof...( Functions ) > 0,
			               "fork_join requires at least one fork function" );

			template<typename F>
			using fork_result_t = daw::remove_cvref_t<decltype( std::apply(
			  std::declval<F &>( ), std::declval<ArgsTuple const &>( ) ) )>;

			static_assert( ( not std::is_void_v<fork_result_t<Functions>> and ... ),
			               "fork_join fork functions must return a value to pass "
			               "to the joiner" );

			static_assert(
			  std::is_invocable_v<Joiner &, fork_result_t<Functions>...>,
			  "fork_join joiner must be callable with the results of the fork "
			  "functions, in order" );

			using join_result_t = daw::remove_cvref_t<
			  std::invoke_result_t<Joiner &, fork_result_t<Functions>...>>;

			ArgsTuple args;
			Joiner joiner;
			std::tuple<Functions...> funcs;
			std::tuple<daw::expected_t<fork_result_t<Functions>>...> results{ };
			std::atomic<std::size_t> remaining{ sizeof...( Functions ) };
			future_result_t<join_result_t> out;

			fork_join_state_t( ArgsTuple a, Joiner j, std::tuple<Functions...> fs,
			                   future_result_t<join_result_t> o )
			  : args( std::move( a ) )
			  , joiner( std::move( j ) )
			  , funcs( std::move( fs ) )
			  , out( std::move( o ) ) {}

			static void start( std::shared_ptr<fork_join_state_t> const &st,
			                   task_scheduler &ts ) {
				[&]<std::size_t... Is>( std::index_sequence<Is...> ) {
					( schedule<Is>( st, ts ), ... );
				}( std::index_sequence_for<Functions...>{ } );
			}

		private:
			template<std::size_t N>
			static void schedule( std::shared_ptr<fork_join_state_t> const &st,
			                      task_scheduler &ts ) {
				if( not ts.add_task( [st]( ) {
					    st->template run<N>( );
				    } ) ) {
					// The scheduler would not take the task (e.g. it is stopping).
					// Run it here so that the joined future is still completed
					st->template run<N>( );
				}
			}

			template<std::size_t N>
			void run( ) {
				std::get<N>( results ) = daw::expected_from_code( [&]( ) {
					return std::apply( std::get<N>( funcs ), std::as_const( args ) );
				} );
				if( remaining.fetch_sub( 1, std::memory_order_acq_rel ) == 1 ) {
					join( );
				}
			}

			void join( ) {
				// get( ) rethrows if a fork function threw; from_code then stores
				// that exception in out
				out.from_code( [&]( ) -> join_result_t {
					return std::apply(
					  [&]( auto &...ex ) -> join_result_t {
						  return joiner( std::move( ex ).get( )... );
					  },
					  results );
				} );
			}
		};

		/// Installs a fork_join continuation in a member_data_t.  Shared by the
		/// void and non-void specializations, which befriend it for pass_next
		struct fork_join_installer {
		template<typename MemberData, typename ArgsTuple, typename Function,
		         typename... Functions>
		[[nodiscard]] static auto install( MemberData &self, Function &&joiner,
		                                   Functions &&...funcs ) {
			using state_t =
			  fork_join_state_t<daw::remove_cvref_t<Function>, ArgsTuple,
			                    daw::remove_cvref_t<Functions>...>;
			using join_result_t = typename state_t::join_result_t;
			using expected_result_t = typename MemberData::expected_result_t;

			assert( self.m_data );
			auto result =
			  future_result_t<join_result_t>( self.m_data->m_task_scheduler );

			auto nxt = self.m_data->m_next.get( );
			daw::exception::precondition_check( not( *nxt ),
			                                    "Can only set next function once" );

			*nxt = [out = result,
			        jn = daw::remove_cvref_t<Function>( DAW_FWD( joiner ) ),
			        fns = std::tuple<daw::remove_cvref_t<Functions>...>(
			          DAW_FWD( funcs )... ),
			        ts = self.m_data->m_task_scheduler](
			         expected_result_t value ) mutable {
				if( value.has_exception( ) ) {
					out.set_exception( value.get_exception_ptr( ) );
					return;
				}
				auto args = [&] {
					if constexpr( std::tuple_size_v<ArgsTuple> == 0 ) {
						return ArgsTuple( );
					} else {
						return ArgsTuple( std::move( value ).get( ) );
					}
				}( );
				// The continuation only runs once, so jn and fns can be moved from
				state_t::start( std::make_shared<state_t>( std::move( args ),
				                                           std::move( jn ),
				                                           std::move( fns ),
				                                           std::move( out ) ),
				                ts );
			};
			// Check for ready before marking continued, as next( ) does.  set_value
			// holds the m_next lock while publishing, so this cannot race with it
			if( future_status::ready == self.m_data->status( ) ) {
				self.pass_next( std::move( self.m_data->m_result ) );
				self.m_data->status( future_status::continued );
			} else {
				self.m_data->status( future_status::continued );
				nxt.release( );
				self.m_data->notify( );
			}
			return result;
		}
		};

		template<typename Result>
		struct [[nodiscard]] member_data_t {
			using base_result_t = Result;
			using expected_result_t = daw::expected_t<base_result_t>;
			using next_function_t = std::function<void( expected_result_t )>;

			using data_t =
			  impl::member_data_members<expected_result_t, next_function_t>;

			std::shared_ptr<data_t> m_data;

			friend struct fork_join_installer;

			explicit member_data_t( task_scheduler ts )
			  : m_data( std::make_shared<data_t>( std::move( ts ) ) ) {}

			explicit member_data_t( daw::shared_cnt_sem sem, task_scheduler ts )
			  : m_data(
			      std::make_shared<data_t>( std::move( sem ), std::move( ts ) ) ) {}

			// DAW DAW DAW
			~member_data_t( ) = default;
			member_data_t( member_data_t const & ) = default;
			member_data_t &operator=( member_data_t const & ) = default;
			/*
			member_data_t( member_data_t && ) = default copy;
			member_data_t &operator=( member_data_t && ) = default copy;
			*/

		private:
			explicit member_data_t( std::shared_ptr<data_t> &&dptr ) noexcept
			  : m_data( std::move( dptr ) ) {}

			[[nodiscard]] decltype( auto ) pass_next( expected_result_t &&value ) {
				auto nxt = m_data->m_next.get( );
				daw::exception::precondition_check(
				  *nxt, "Attempt to call next function on empty function" );

				return ( *nxt )( std::move( value ) );
			}

			[[nodiscard]] decltype( auto )
			pass_next( expected_result_t const &value ) {

				auto nxt = m_data->m_next.get( );
				daw::exception::precondition_check(
				  *nxt, "Attempt to call next function on empty function" );

				return ( *nxt )( value );
			}

		public:
			void set_value( expected_result_t &&value ) {
				assert( m_data );
				// Hold the lock on m_next until the result is published so that a
				// concurrent next( ) cannot attach a continuation that is never called
				auto nxt = m_data->m_next.get( );
				if( *nxt ) {
					pass_next( std::move( value ) );
					return;
				}
				m_data->m_result = std::move( value );
				m_data->status( future_status::ready );
				m_data->notify( );
			} // namespace impl

			void set_value( expected_result_t const &value ) {
				assert( m_data );
				// Hold the lock on m_next until the result is published so that a
				// concurrent next( ) cannot attach a continuation that is never called
				auto nxt = m_data->m_next.get( );
				if( *nxt ) {
					pass_next( value );
					return;
				}
				m_data->m_result = value;
				m_data->status( future_status::ready );
				m_data->notify( );
			}

			void set_value( base_result_t &&value ) {
				assert( m_data );
				// Hold the lock on m_next until the result is published so that a
				// concurrent next( ) cannot attach a continuation that is never called
				auto nxt = m_data->m_next.get( );
				if( *nxt ) {
					pass_next(
					  daw::construct_a<expected_result_t>( std::move( value ) ) );
					return;
				}
				m_data->m_result = std::move( value );
				m_data->status( future_status::ready );
				m_data->notify( );
			}

			void set_value( base_result_t const &value ) {
				assert( m_data );
				// Hold the lock on m_next until the result is published so that a
				// concurrent next( ) cannot attach a continuation that is never called
				auto nxt = m_data->m_next.get( );
				if( *nxt ) {
					pass_next( daw::construct_a<expected_result_t>( value ) );
					return;
				}
				m_data->m_result = value;
				m_data->status( future_status::ready );
				m_data->notify( );
			}

			void set_exception( std::exception_ptr ptr ) {
				assert( m_data );
				// Hold the lock on m_next until the result is published so that a
				// concurrent next( ) cannot attach a continuation that is never called
				auto nxt = m_data->m_next.get( );
				if( *nxt ) {
					pass_next( daw::construct_a<expected_result_t>( ptr ) );
					return;
				}
				m_data->m_result = ptr;
				m_data->status( future_status::ready );
				m_data->notify( );
			}

			[[nodiscard]] bool is_exception( ) const {
				assert( m_data );
				m_data->wait( );
				// TODO: look into not throwing and allowing values to be retrieved
				daw::exception::daw_throw_on_true(
				  m_data->status( ) == future_status::continued,
				  "Attempt to use a future that has been continued" );
				return m_data->m_result.has_exception( );
			}

			[[nodiscard]] decltype( auto ) get( ) {
				assert( m_data );
				m_data->wait( );
				daw::exception::daw_throw_on_true(
				  m_data->status( ) == future_status::continued,
				  "Attempt to use a future that has been continued" );
				return m_data->m_result.get( );
			}

			[[nodiscard]] decltype( auto ) get( ) const {
				assert( m_data );
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
			void from_code( Function &&func, Args &&...args ) {
				try {
					set_value(
					  expected_from_code( DAW_FWD( func ), DAW_FWD( args )... ) );
				} catch( ... ) { set_exception( std::current_exception( ) ); }
			}

			template<typename Function>
			requires( not std::is_function_v<std::remove_reference_t<Function>> ) //
			[[nodiscard]] auto next( Function &&func ) {
				assert( m_data );
				auto nxt = m_data->m_next.get( );
				assert( not( *nxt ) ); // can only set next function once

				using next_result_t =
				  decltype( func( std::declval<base_result_t>( ) ) );

				auto result =
				  future_result_t<next_result_t>( m_data->m_task_scheduler );

				*nxt = [res = daw::mutable_capture( result ),
				        fn = daw::mutable_capture( DAW_FWD( func ) ),
				        ts = daw::mutable_capture( m_data->m_task_scheduler ),
				        self = *this]( expected_result_t value ) -> void {
					if( not value.has_value( ) ) {
						res->set_exception( value.get_exception_ptr( ) );
						return;
					}
					if( not ts->add_task(
					      [rslt = daw::mutable_capture( res.move_out( ) ),
								 fun = daw::mutable_capture( fn.move_out( ) ),
								 v = daw::mutable_capture( std::move( value ).get( ) )]( ) {
						      rslt->from_code( fun.move_out( ), v.move_out( ) );
					      } ) ) {

						throw daw::unable_to_add_task_exception{ };
					}
				};
				if( future_status::ready == m_data->status( ) ) {
					pass_next( std::move( m_data->m_result ) );
					m_data->status( future_status::continued );
				} else {
					m_data->status( future_status::continued );
					nxt.release( );
					m_data->notify( );
				}
				return result;
			}

			template<typename... Functions>
			[[nodiscard]] auto fork( Functions &&...funcs ) {
				assert( m_data );
				auto nxt = m_data->m_next.get( );
				assert( not( *nxt ) ); // can only set next function once

				using result_t = std::tuple<future_result_t<DAW_TYPEOF(
				  funcs( std::declval<expected_result_t>( ).get( ) ) )>...>;

				auto const construct_future = [&]( auto &&f ) {
					Unused( f );
					using fut_t = future_result_t<decltype( f(
					  std::declval<expected_result_t>( ).get( ) ) )>;

					return fut_t( m_data->m_task_scheduler );
				};
				auto result = result_t( construct_future( funcs )... );
				*nxt = [res = mutable_capture( result ),
				        tpfns = daw::mutable_capture(
				          std::tuple<daw::remove_cvref_t<Functions>...>(
				            DAW_FWD( funcs )... ) ),
				        ts = daw::mutable_capture( m_data->m_task_scheduler ),
				        self = *this]( cvref_of<expected_result_t> auto &&value ) {
					if( value.has_value( ) ) {
						if( not ts->add_task(
						      impl::add_fork_task( *ts, *res, *tpfns, value.get( ) ) ) ) {

							throw daw::unable_to_add_task_exception{ };
						}
					} else {
						daw::tuple::apply( *res,
						                   [ptr = value.get_exception_ptr( )]( auto &t ) {
							                   t.set_exception( ptr );
						                   } );
					}
				};
				if( future_status::ready == m_data->status( ) ) {
					pass_next( std::move( m_data->m_result ) );
					m_data->status( future_status::continued );
				} else {
					m_data->status( future_status::continued );
					nxt.release( );
					m_data->notify( );
				}
				return result;
			}

			template<typename Function, typename... Functions>
			[[nodiscard]] auto fork_join( Function &&joiner, Functions &&...funcs ) {
				static_assert(
				  ( std::is_invocable_v<Functions &, base_result_t const &> and ... ),
				  "Each fork function must be callable with the future's value" );
				return impl::fork_join_installer::install<member_data_t, std::tuple<base_result_t>>(
				  *this, DAW_FWD( joiner ), DAW_FWD( funcs )... );
			}

			void wait( ) const {
				assert( m_data );
				m_data->wait( );
			}

			[[nodiscard]] bool try_wait( ) const {
				assert( m_data );
				return m_data->try_wait( );
			}

			[[nodiscard]] auto get_handle( ) const {
				assert( m_data );
				class handle_t {
					std::weak_ptr<data_t> m_handle;

					explicit handle_t( std::weak_ptr<data_t> wptr )
					  : m_handle( wptr ) {}

					friend member_data_t;

				public:
					[[nodiscard]] bool expired( ) const {
						return m_handle.expired( );
					}

					[[nodiscard]] std::optional<member_data_t> lock( ) const {
						if( auto lck = m_handle.lock( ); lck ) {
							auto m = member_data_t( std::move( lck ) );
							return std::optional<member_data_t>( std::move( m ) );
						}
						return { };
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

			friend struct fork_join_installer;

			explicit member_data_t( task_scheduler ts )
			  : m_data( std::make_shared<data_t>( std::move( ts ) ) ) {}

			explicit member_data_t( daw::shared_cnt_sem sem, task_scheduler ts )
			  : m_data(
			      std::make_shared<data_t>( std::move( sem ), std::move( ts ) ) ) {}

		private:
			explicit member_data_t( std::shared_ptr<data_t> &&dptr ) noexcept
			  : m_data( std::move( dptr ) ) {}

			void pass_next( expected_result_t &&value ) {
				auto nxt = m_data->m_next.get( );
				daw::exception::precondition_check(
				  *nxt, "Attempt to call next function on empty function" );

				daw::exception::precondition_check(
				  not value.has_exception( ), "Unexpected exception in expected_t" );

				( *nxt )( std::move( value ) );
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
			void from_code( Function &&func, Args &&...args ) {
				static_assert( traits::is_callable_v<Function, Args...>,
				               "Cannot call func with args provided" );
				try {
					func( DAW_FWD( args )... );
					set_value( );
				} catch( ... ) { set_exception( std::current_exception( ) ); }
			}

			template<typename Function>
			[[nodiscard]] auto next( Function &&func ) {
				auto nxt = m_data->m_next.get( );
				daw::exception::precondition_check( not *nxt,
				                                    "Can only set next function once" );
				using next_result_t =
				  decltype( std::declval<std::remove_reference_t<Function>>( )( ) );

				auto result =
				  future_result_t<next_result_t>( m_data->m_task_scheduler );

				*nxt = [rslt = daw::mutable_capture( result ),
				        fn = daw::mutable_capture( DAW_FWD( func ) ),
				        ts = daw::mutable_capture( m_data->m_task_scheduler ),
				        self = *this]( expected_result_t value ) -> void {
					if( value.has_value( ) ) {
						if( not ts->add_task(
						      [res = daw::mutable_capture( rslt.move_out( ) ),
									 fun = daw::mutable_capture( fn.move_out( ) )]( ) {
							      res->from_code( fun.move_out( ) );
						      } ) ) {

							throw daw::unable_to_add_task_exception{ };
						}
					} else {
						rslt->set_exception( value.get_exception_ptr( ) );
					}
				};
				if( future_status::ready == m_data->status( ) ) {
					pass_next( std::move( m_data->m_result ) );
					m_data->status( future_status::continued );
				} else {
					m_data->status( future_status::continued );
					nxt.release( );
					m_data->notify( );
				}
				return result;
			}

			template<typename... Functions>
			[[nodiscard]] auto fork( Functions &&...funcs ) {
				auto nxt = m_data->m_next.get( );
				daw::exception::precondition_check( not( *nxt ),
				                                    "Can only set next function once" );
				using result_t = std::tuple<
				  future_result_t<daw::remove_cvref_t<decltype( funcs( ) )>>...>;

				auto const construct_future = [&]( auto &&f ) {
					Unused( f );
					using fut_t = future_result_t<decltype( f( ) )>;
					return fut_t( m_data->m_task_scheduler );
				};

				auto result =
				  daw::construct_a<result_t>( construct_future( funcs )... );

				auto tpfuncs =
				  std::tuple<daw::remove_cvref_t<Functions>...>( DAW_FWD( funcs )... );
				*nxt = [result,
				        tpfns = std::move( tpfuncs ),
				        ts = m_data->m_task_scheduler,
				        self =
				          *this]( cvref_of<expected_result_t> auto &&value ) mutable {
					if( value.has_value( ) ) {
						ts.add_task( impl::add_fork_task( ts, result, tpfns ) );
					} else {
						daw::tuple::apply( result,
						                   [ptr = value.get_exeption_ptr( )]( auto &&t ) {
							                   t.set_exception( ptr );
						                   } );
					}
				};
				if( future_status::ready == m_data->status( ) ) {
					pass_next( std::move( m_data->m_result ) );
					m_data->status( future_status::continued );
				} else {
					m_data->status( future_status::continued );
					nxt.release( );
					m_data->notify( );
				}
				return result;
			}

			template<typename Function, typename... Functions>
			[[nodiscard]] auto fork_join( Function &&joiner, Functions &&...funcs ) {
				static_assert( ( std::is_invocable_v<Functions &> and ... ),
				               "Each fork function must be callable with no arguments" );
				return impl::fork_join_installer::install<member_data_t, std::tuple<>>(
				  *this, DAW_FWD( joiner ), DAW_FWD( funcs )... );
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
					  : m_handle( std::move( wptr ) ) {}

					friend member_data_t;

				public:
					[[nodiscard]] bool expired( ) const {
						return m_handle.expired( );
					}

					[[nodiscard]] std::optional<member_data_t> lock( ) const {
						if( auto lck = m_handle.lock( ); lck ) {
							return member_data_t( std::move( lck ) );
						}
						return { };
					}
				};

				return handle_t( static_cast<std::weak_ptr<data_t>>( m_data ) );
			}
		};

		struct [[nodiscard]] future_result_base_t {
			using i_am_a_future_result = void;
			future_result_base_t( ) = default;
			future_result_base_t( future_result_base_t const & ) noexcept = default;
			future_result_base_t( future_result_base_t && ) noexcept = default;
			future_result_base_t &
			operator=( future_result_base_t const & ) noexcept = default;
			future_result_base_t &
			operator=( future_result_base_t && ) noexcept = default;

			virtual ~future_result_base_t( );
			virtual void wait( ) const = 0;
			[[nodiscard]] virtual bool try_wait( ) const = 0;
		}; // future_result_base_t

		template<size_t N, size_t SZ, typename... Callables>
		struct [[nodiscard]] apply_many_t {
			template<typename... ResultTypes, typename... Args>
			void operator( )( daw::task_scheduler &ts, daw::shared_cnt_sem sem,
			                  std::tuple<ResultTypes...> &results,
			                  std::tuple<Callables...> const &callables,
			                  std::shared_ptr<std::tuple<Args...>> const &tp_args ) {

				// TODO this looks weird
				if( not schedule_task(
				      sem,
				      [res = std::addressof( results ),
							 call = std::addressof( callables ),
							 tp_args]( ) {
					      try {
						      std::get<N>( *res ) =
						        std::apply( std::get<N>( *call ), *tp_args );
					      } catch( ... ) { std::get<N>( *res ).set_exception( ); }
				      },
				      ts ) ) {

					throw daw::unable_to_add_task_exception{ };
				} // namespace impl

				apply_many_t<N + 1, SZ, Callables...>{ }(
				  ts, sem, results, callables, tp_args );
			} // namespace daw
		};

		template<size_t SZ, typename... Functions>
		struct [[nodiscard]] apply_many_t<SZ, SZ, Functions...> {
			template<typename Results, typename... Args>
			constexpr void
			operator( )( daw::task_scheduler const &, daw::shared_cnt_sem const &,
			             Results const &, std::tuple<Functions...> const &,
			             std::shared_ptr<std::tuple<Args...>> const & ) {}
		}; // apply_many_t<SZ, SZ, Functions..>

		template<typename Result, typename... Functions, typename... Args>
		void apply_many( daw::task_scheduler &ts, daw::shared_cnt_sem sem,
		                 Result &result, std::tuple<Functions...> const &callables,
		                 std::shared_ptr<std::tuple<Args...>> tp_args ) {

			apply_many_t<0, sizeof...( Functions ), Functions...>{ }(
			  ts, sem, result, callables, tp_args );
		}

		template<typename... Functions>
		class [[nodiscard]] future_group_result_t {
			std::tuple<Functions...> tp_functions;

		public:
			template<
			  typename... Fs,
			  daw::enable_when_t<
			    ( sizeof...( Fs ) != 1 or
					  not std::is_same_v<
			        future_group_result_t,
			        daw::remove_cvref_t<daw::traits::first_type<Fs...>>> )> = nullptr>
			explicit constexpr future_group_result_t( Fs &&...fs )
			  : tp_functions( DAW_FWD( fs )... ) {}

			template<typename... Args>
			[[nodiscard]] auto operator( )( Args &&...args ) {
				using result_tp_t = std::tuple<daw::expected_t<daw::remove_cvref_t<
				  decltype( std::declval<Functions>( )( DAW_FWD( args )... ) )>>...>;

				// Copy arguments to const, non-ref, non-volatile versions in a
				// shared_pointer so that only one copy is ever created
				auto tp_args = std::make_shared<
				  std::tuple<std::add_const_t<daw::remove_cvref_t<Args>>...>>(
				  DAW_FWD( args )... );

				auto ts = get_task_scheduler( );
				auto result = future_result_t<result_tp_t>( ts );

				auto th_worker = [res = daw::mutable_capture( result ),
				                  tp_funcs = daw::mutable_capture( tp_functions ),
				                  tsk_sch = daw::mutable_capture( ts ),
				                  tp_arg =
				                    daw::mutable_capture( std::move( tp_args ) )]( ) {
					// tp_result and funcs are referenced by the sub tasks, so they must
					// outlive them.  We wait on sem below before leaving this scope
					auto tp_result = result_tp_t( );
					auto const funcs = tp_funcs.move_out( );
					// Each scheduled sub task adds a notifier, the initial count of 1 is
					// released once they have all been scheduled
					auto sem = daw::shared_cnt_sem( 1 );
					{
						auto const oe = daw::on_scope_exit( [sem]( ) mutable {
							sem.notify( );
						} );
						impl::apply_many( *tsk_sch, sem, tp_result, funcs,
						                  tp_arg.move_out( ) );
					}
					sem.wait( );
					res->set_value( std::move( tp_result ) );
				};
				try {
					if( not ts.add_task( std::move( th_worker ) ) ) {
						throw daw::unable_to_add_task_exception{ };
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
