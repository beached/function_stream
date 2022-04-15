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

#include "impl/future_result_impl.h"
#include "task_scheduler.h"

#include <daw/cpp_17.h>
#include <daw/daw_exception.h>
#include <daw/daw_expected.h>
#include <daw/daw_function.h>
#include <daw/daw_traits.h>
#include <daw/parallel/daw_latch.h>

#include <chrono>
#include <list>
#include <memory>
#include <tuple>
#include <utility>

namespace daw {
	template<typename Result>
	struct [[nodiscard]] future_result_t : public impl::future_result_base_t {
		using result_type_t = daw::remove_cvref_t<Result>;
		using result_t = daw::expected_t<result_type_t>;
		using m_data_t = impl::member_data_t<result_type_t>;

	private:
		m_data_t m_data = m_data_t( get_task_scheduler( ) );

		explicit future_result_t( m_data_t d ) noexcept
		  : m_data( std::move( d ) ) {}

	public:
		future_result_t( ) = default;

		explicit future_result_t( task_scheduler ts )
		  : m_data( ::daw::move( ts ) ) {}

		explicit future_result_t( daw::shared_latch sem, task_scheduler ts )
		  : m_data( ::daw::move( sem ), ::daw::move( ts ) ) {}

		explicit future_result_t( daw::shared_latch sem )
		  : m_data( ::daw::move( sem ), get_task_scheduler( ) ) {}

		future_result_t( future_result_t const & ) = default;
		future_result_t &operator=( future_result_t const & ) = default;
		future_result_t( future_result_t &&other )
		  : m_data( other.m_data ) {}
		future_result_t &operator=( future_result_t &&rhs ) {
			m_data = rhs.m_data;
			return *this;
		}
		~future_result_t( ) override = default;

		[[nodiscard]] auto get_handle( ) const {
			using data_handle_t =
			  daw::remove_cvref_t<decltype( m_data.get_handle( ) )>;

			class handle_t {
				data_handle_t m_handle;

				explicit handle_t( data_handle_t hnd )
				  : m_handle( std::move( hnd ) ) {}

				friend future_result_t<Result>;

			public:
				using type = result_type_t;

				[[nodiscard]] bool expired( ) const {
					return m_handle.expired( );
				}

				[[nodiscard]] std::optional<future_result_t> lock( ) const {
					if( auto lck = m_handle.lock( ); lck ) {
						return future_result_t( std::move( *lck ) );
					}
					return { };
				}
			};

			return handle_t( m_data.get_handle( ) );
		}

		void wait( ) const override {
			m_data.wait( );
		}

		template<typename Rep, typename Period>
		[[nodiscard]] future_status
		wait_for( std::chrono::duration<Rep, Period> rel_time ) const {
			return m_data.wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] future_status
		wait_until( std::chrono::time_point<Clock, Duration> timeout_time ) const {
			return m_data.wait_until( timeout_time );
		}

		[[nodiscard]] bool try_wait( ) const override {
			return m_data.try_wait( );
		}

		[[nodiscard]] explicit operator bool( ) const {
			return m_data.try_wait( );
		}

		template<typename R>
		void set_value( R &&value ) {
			static_assert( std::is_convertible_v<daw::remove_cvref_t<R>, Result>,
			               "Argument must convertible to a Result type" );
			m_data.set_value( std::forward<R>( value ) );
		}

		template<typename Exception>
		void set_exception( Exception &&ex ) {
			m_data.set_exception(
			  std::make_exception_ptr( std::forward<Exception>( ex ) ) );
		}

		void set_exception( ) {
			m_data.set_exception( std::current_exception( ) );
		}

		void set_exception( std::exception_ptr ptr ) {
			m_data.set_exception( ptr );
		}

		template<typename Function, typename... Args>
		void from_code( Function &&func, Args &&...args ) {
			static_assert(
			  std::is_convertible_v<decltype( ::std::forward<Function>( func )(
			                          ::std::forward<Args>( args )... ) ),
			                        Result>,
			  "Function func with Args does not return a value that is "
			  "convertible to Result. e.g Result "
			  "r = func( args... ) must be valid" );
			m_data.from_code( daw::make_callable( std::forward<Function>( func ) ),
			                  std::forward<Args>( args )... );
		}

		[[nodiscard]] bool is_exception( ) const {
			return m_data.is_exception( );
		}

		[[nodiscard]] decltype( auto ) get( ) {
			return m_data.get( );
		}

		[[nodiscard]] decltype( auto ) get( ) const {
			return m_data.get( );
		}

		template<typename Function>
		[[nodiscard]] decltype( auto ) next( Function &&func ) {
			return m_data.next(
			  daw::make_callable( std::forward<Function>( func ) ) );
		}

		template<typename... Functions>
		[[nodiscard]] decltype( auto ) fork( Functions &&...funcs ) {
			return m_data.fork(
			  daw::make_callable( std::forward<Functions>( funcs ) )... );
		}

		template<typename Function, typename... Functions>
		[[nodiscard]] decltype( auto ) fork_join( Function &&joiner,
		                                          Functions &&...funcs ) {
			return m_data.fork_join(
			  daw::make_callable( std::forward<Function>( joiner ) ),
			  daw::make_callable( std::forward<Functions>( funcs ) )... );
		}
	};
	// future_result_t

	template<>
	struct [[nodiscard]] future_result_t<void>
	  : public impl::future_result_base_t {
		using result_type_t = void;
		using result_t = daw::expected_t<result_type_t>;
		using m_data_t = impl::member_data_t<result_type_t>;

	private:
		m_data_t m_data = m_data_t( get_task_scheduler( ) );

		explicit future_result_t( m_data_t d ) noexcept
		  : m_data( std::move( d ) ) {}

	public:
		future_result_t( ) = default;
		explicit future_result_t( task_scheduler ts );
		explicit future_result_t( daw::shared_latch sem,
		                          task_scheduler ts = get_task_scheduler( ) );

		[[nodiscard]] auto get_handle( ) const {
			using data_handle_t =
			  daw::remove_cvref_t<decltype( m_data.get_handle( ) )>;
			class handle_t {
				data_handle_t m_handle;

				explicit handle_t( data_handle_t hnd )
				  : m_handle( std::move( hnd ) ) {}

				friend future_result_t;

			public:
				using type = void;

				[[nodiscard]] bool expired( ) const {
					return m_handle.expired( );
				}

				[[nodiscard]] std::optional<future_result_t> lock( ) const {
					if( auto lck = m_handle.lock( ); lck ) {
						return future_result_t( std::move( *lck ) );
					}
					return { };
				}
			};

			return handle_t( m_data.get_handle( ) );
		}

		void wait( ) const override;

		template<typename Rep, typename Period>
		[[nodiscard]] future_status
		wait_for( std::chrono::duration<Rep, Period> rel_time ) const {
			return wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		[[nodiscard]] future_status
		wait_until( std::chrono::time_point<Clock, Duration> timeout_time ) const {
			return wait_until( timeout_time );
		}

		void get( ) const;
		[[nodiscard]] bool try_wait( ) const override;
		[[nodiscard]] explicit operator bool( ) const;
		[[nodiscard]] bool is_exception( ) const;

		void set_value( );
		void set_exception( );
		void set_exception( std::exception_ptr ptr );

		template<typename Exception>
		void set_exception( Exception &&ex ) {
			m_data.set_exception(
			  std::make_exception_ptr( std::forward<Exception>( ex ) ) );
		}

		template<typename Function, typename... Args>
		void from_code( Function &&func, Args &&...args ) {
			m_data.from_code( daw::make_void_function( daw::make_callable(
			                    std::forward<Function>( func ) ) ),
			                  std::forward<Args>( args )... );
		}

		template<typename Function>
		[[nodiscard]] decltype( auto ) next( Function &&function ) {
			return m_data.next(
			  daw::make_callable( std::forward<Function>( function ) ) );
		}

		template<typename Function, typename... Functions>
		[[nodiscard]] decltype( auto ) fork( Function &&func,
		                                     Functions &&...funcs ) const {
			return m_data.fork( daw::make_callable(
			  std::forward<Function>( func ), std::forward<Functions>( funcs ) )... );
		}
	}; // future_result_t<void>

	template<typename T>
	future_result_t( T ) -> future_result_t<T>;

	template<typename result_t, typename Function>
	[[nodiscard]] constexpr decltype( auto )
	operator|( future_result_t<result_t> &lhs, Function &&rhs ) {
		static_assert( daw::traits::is_callable_v<Function, result_t>,
		               "Supplied function must be callable with result of future" );
		return lhs.next( daw::make_callable( std::forward<Function>( rhs ) ) );
	}

	/*
	 * TODO: get rid of me
	template<typename result_t, typename Function>
	[[nodiscard]] constexpr decltype( auto )
	operator|( future_result_t<result_t> const &lhs, Function &&rhs ) {
	  static_assert( daw::traits::is_callable_v<Function, result_t>,
	                 "Supplied function must be callable with result of future" );
	  return lhs.next( daw::make_callable( std::forward<Function>( rhs ) ) );
	}*/

	template<typename result_t, typename Function>
	[[nodiscard]] constexpr decltype( auto )
	operator|( future_result_t<result_t> &&lhs, Function &&rhs ) {
		static_assert(
		  daw::traits::is_callable_v<std::remove_reference_t<Function>, result_t>,
		  "Supplied function must be callable with result of future" );
		return lhs.next( daw::make_callable( std::forward<Function>( rhs ) ) );
	}

	template<typename Function, typename... Args,
	         std::enable_if_t<daw::traits::is_callable_v<Function, Args...>,
	                          std::nullptr_t> = nullptr>
	[[nodiscard]] auto make_future_result( task_scheduler ts, Function &&func,
	                                       Args &&...args ) {
		using result_t =
		  daw::remove_cvref_t<decltype( func( std::forward<Args>( args )... ) )>;
		auto result = future_result_t<result_t>( );

		if( not ts.add_task( [result = daw::mutable_capture( result ),
		                      func = daw::mutable_capture( daw::make_callable(
		                        std::forward<Function>( func ) ) ),
		                      args = daw::mutable_capture( std::make_tuple(
		                        std::forward<Args>( args )... ) )]( ) -> void {
			    result->from_code(
			      [func = daw::mutable_capture( daw::move( *func ) ),
			       args = daw::mutable_capture( daw::move( *args ) )]( ) {
				      return daw::apply( daw::move( *func ), daw::move( *args ) );
			      } );
		    } ) ) {
			throw ::daw::unable_to_add_task_exception{ };
		}
		return result;
	}

	namespace impl {
		template<typename Result, typename Function, typename... Args>
		struct future_task_t {
			Result result;
			Function func;
			std::tuple<Args...> args;

			template<typename R, typename F, typename... A>
			constexpr future_task_t( R &&r, F &&f, A &&...a )
			  : result( std::forward<R>( r ) )
			  , func( std::forward<F>( f ) )
			  , args( std::forward<A>( a )... ) {}

			void operator( )( ) {
				try {
					result.set_value(
					  daw::apply( daw::move( func ), daw::move( args ) ) );
				} catch( ... ) { result.set_exception( ); }
			}
		};

		template<typename Result, typename Function, typename... Args>
		[[nodiscard]] constexpr future_task_t<Result, Function, Args...>
		make_future_task( Result &&result, Function &&func, Args &&...args ) {
			return { std::forward<Result>( result ),
			         daw::make_callable( std::forward<Function>( func ) ),
			         std::forward<Args>( args )... };
		}
	} // namespace impl

	template<typename Function, typename... Args>
	[[nodiscard]] auto make_future_result( task_scheduler ts,
	                                       daw::shared_latch sem, Function &&func,
	                                       Args &&...args ) {

		static_assert(
		  daw::traits::is_callable_v<std::remove_reference_t<Function>, Args...> );
		using result_t = decltype( std::forward<Function>( func )(
		  std::forward<Args>( args )... ) );
		auto result = future_result_t<result_t>( daw::move( sem ) );
		ts.add_task( impl::make_future_task(
		  result, daw::make_callable( std::forward<Function>( func ) ),
		  std::forward<Args>( args )... ) );

		return result;
	} // namespace daw

	template<typename Function, typename... Args>
	[[nodiscard]] decltype( auto ) make_future_result( Function &&func,
	                                                   Args &&...args ) {
		static_assert(
		  daw::traits::is_callable_v<std::remove_reference_t<Function>, Args...> );
		return make_future_result(
		  get_task_scheduler( ),
		  daw::make_callable( std::forward<Function>( func ) ),
		  std::forward<Args>( args )... );
	}

	template<typename Function, typename... Args>
	[[nodiscard]] decltype( auto ) async( Function &&func, Args &&...args ) {
		static_assert(
		  daw::traits::is_callable_v<std::remove_reference_t<Function>, Args...> );
		return make_future_result( std::forward<Function>( func ),
		                           std::forward<Args>( args )... );
	}

	namespace async_impl {
		template<typename... Functions>
		[[nodiscard]] decltype( auto )
		make_callable_future_result_group_impl( Functions &&...functions ) {
			return impl::future_group_result_t<daw::remove_cvref_t<Functions>...>(
			  std::forward<Functions>( functions )... );
		}
	} // namespace async_impl
	template<typename... Functions>
	[[nodiscard]] decltype( auto )
	make_callable_future_result_group( Functions &&...functions ) {
		return async_impl::make_callable_future_result_group_impl(
		  daw::make_callable( std::forward<Functions>( functions ) )... );
	}

	/// Create a group of functions that all return at the same time.  A tuple of
	/// results is returned
	//
	//  @param functions a list of functions of form Result( )
	template<typename... Functions>
	[[nodiscard]] decltype( auto )
	make_future_result_group( Functions &&...functions ) {
		return make_callable_future_result_group(
		  daw::make_callable( std::forward<Functions>( functions ) )... )( );
	}

	std::false_type is_future_result_impl( ... );

	template<typename T>
	[[nodiscard]] inline constexpr std::true_type
	is_future_result_impl( future_result_t<T> const & );

	template<typename T>
	inline constexpr bool is_future_result_v =
	  decltype( is_future_result_impl( std::declval<T>( ) ) )::value;

	namespace impl {
		template<typename Iterator, typename OutputIterator, typename BinaryOp>
		inline OutputIterator reduce_futures2( Iterator first, Iterator last,
		                                       OutputIterator out_it,
		                                       BinaryOp const &binary_op ) {
			auto const sz = std::distance( first, last );
			assert( sz >= 0 );
			if( sz <= 0 ) {
				return out_it;
			} else if( sz == 1 ) {
				*out_it++ = *first++;
				return out_it;
			}
			bool const odd_count = sz % 2 == 1;
			if( odd_count ) {
				last = std::next( first, sz - 1 );
			}
			while( first != last ) {
				auto l_it = first++;
				auto r_it = first++;
				*out_it++ = l_it->next(
				  [r = daw::mutable_capture( *r_it ),
				   binary_op = daw::mutable_capture( binary_op )]( auto &&result ) {
					  return ( *binary_op )( std::forward<decltype( result )>( result ),
					                         r->get( ) );
				  } );
			}
			if( odd_count ) {
				*out_it++ = *last;
			}
			return out_it;
		}
	} // namespace impl

	template<typename RandomIterator, typename RandomIterator2,
	         typename BinaryOperation,
	         typename ResultType = future_result_t<daw::remove_cvref_t<
	           decltype( ( *std::declval<RandomIterator>( ) ).get( ) )>>>
	[[nodiscard]] ResultType reduce_futures( RandomIterator first,
	                                         RandomIterator2 last,
	                                         BinaryOperation &&binary_op ) {

		static_assert( is_future_result_v<decltype( *first )>,
		               "RandomIterator's value type must be a future result" );

		auto results = std::vector<ResultType>( );
		results.reserve( static_cast<size_t>( std::distance( first, last ) ) / 2 );

		impl::reduce_futures2( std::make_move_iterator( first ),
		                       std::make_move_iterator( last ),
		                       std::back_inserter( results ), binary_op );

		while( results.size( ) > 1 ) {
			auto tmp = std::vector<ResultType>( );
			tmp.reserve( results.size( ) / 2 );

			impl::reduce_futures2( std::make_move_iterator( results.begin( ) ),
			                       std::make_move_iterator( results.end( ) ),
			                       std::back_inserter( tmp ), binary_op );
			std::swap( results, tmp );
		}
		return results.front( );
	}

	namespace impl {
		template<typename F, typename Tuple, std::size_t... I>
		[[nodiscard]] constexpr decltype( auto )
		future_apply_impl( F &&f, Tuple &&t, std::index_sequence<I...> ) {
			return std::forward<F>( f )(
			  std::get<I>( std::forward<Tuple>( t ) ).get( )... );
		}
	} // namespace impl

	template<typename F, typename Tuple>
	decltype( auto ) future_apply( F &&f, Tuple &&t ) {
		return impl::future_apply_impl(
		  std::forward<F>( f ), std::forward<Tuple>( t ),
		  std::make_index_sequence<
		    daw::tuple_size_v<daw::remove_cvref_t<Tuple>>>{ } );
	}

	template<typename TPFutureResults, typename Function,
	         std::enable_if_t<daw::traits::is_tuple_v<TPFutureResults>,
	                          std::nullptr_t> = nullptr>
	[[nodiscard]] decltype( auto ) join( TPFutureResults &&results,
	                                     Function &&next_function ) {
		auto result = make_future_result(
		  [results =
		     daw::mutable_capture( std::forward<TPFutureResults>( results ) ),
		   next_function = daw::mutable_capture(
		     daw::make_callable( std::forward<Function>( next_function ) ) )]( ) {
			  return future_apply( daw::move( *next_function ),
			                       daw::move( *results ) );
		  } );
		return result;
	}
} // namespace daw
