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

#include <chrono>
#include <list>
#include <memory>
#include <tuple>
#include <utility>

#include <daw/cpp_17.h>
#include <daw/daw_exception.h>
#include <daw/daw_expected.h>
#include <daw/daw_traits.h>
#include <daw/parallel/daw_latch.h>

#include "impl/future_result_impl.h"
#include "task_scheduler.h"

namespace daw {
	template<typename Result>
	struct future_result_t : public impl::future_result_base_t {
		using result_type_t = std::decay_t<Result>;
		using result_t = daw::expected_t<result_type_t>;
		using m_data_t = impl::member_data_t<result_type_t>;

	private:
		std::shared_ptr<m_data_t> m_data;

	public:
		future_result_t( )
		  : m_data( std::make_shared<m_data_t>( get_task_scheduler( ) ) ) {

			daw::exception::dbg_throw_on_false( m_data, "m_data shouldn't be null" );
		}

		explicit future_result_t( task_scheduler ts )
		  : m_data( std::make_shared<m_data_t>( std::move( ts ) ) ) {

			daw::exception::dbg_throw_on_false( m_data, "m_data shouldn't be null" );
		}

		explicit future_result_t( daw::shared_latch sem,
		                          task_scheduler ts = get_task_scheduler( ) )
		  : m_data(
		      std::make_shared<m_data_t>( std::move( sem ), std::move( ts ) ) ) {

			daw::exception::dbg_throw_on_false( m_data, "m_data shouldn't be null" );
		}

		std::weak_ptr<m_data_t> weak_ptr( ) {
			return m_data;
		}

		void wait( ) const override {
			m_data->wait( );
		}

		template<typename Rep, typename Period>
		future_status
		wait_for( std::chrono::duration<Rep, Period> rel_time ) const {
			return m_data->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		future_status
		wait_until( std::chrono::time_point<Clock, Duration> timeout_time ) const {
			return m_data->wait_until( timeout_time );
		}

		bool try_wait( ) const override {
			return m_data->try_wait( );
		}

		explicit operator bool( ) const {
			return m_data->try_wait( );
		}

		template<typename R>
		void set_value( R &&value ) {
			static_assert( daw::is_convertible_v<R, Result>,
			               "Argument must convertible to a Result type" );
			m_data->set_value( std::forward<R>( value ) );
		}

		template<typename Exception>
		void set_exception( Exception const &ex ) {
			m_data->set_exception( std::make_exception_ptr( ex ) );
		}

		void set_exception( ) {
			m_data->set_exception( std::current_exception( ) );
		}

		void set_exception( std::exception_ptr ptr ) {
			m_data->set_exception( ptr );
		}

		template<typename Function, typename... Args>
		void from_code( Function &&func, Args &&... args ) {
			static_assert(
			  daw::is_convertible_v<daw::traits::result_of_t<Function, Args...>,
			                        Result>,
			  "Function func with Args does not return a value that is "
			  "convertible to Result. e.g Result "
			  "r = func( args... ) must be valid" );
			daw::exception::dbg_throw_on_false( m_data,
			                                    "Expected m_data to be valid" );
			m_data->from_code( std::forward<Function>( func ),
			                   std::forward<Args>( args )... );
		}

		bool is_exception( ) const {
			return m_data->is_exception( );
		}

		decltype( auto ) get( ) {
			return m_data->get( );
		}

		decltype( auto ) get( ) const {
			return m_data->get( );
		}

		template<typename Function>
		decltype( auto ) next( Function &&func ) const {
			return m_data->next( std::forward<Function>( func ) );
		}

		template<typename... Functions>
		decltype( auto ) fork( Functions &&... funcs ) const {
			return m_data->fork( std::forward<Functions>( funcs )... );
		}

		template<typename Function, typename... Functions>
		decltype( auto ) fork_join( Function &&joiner,
		                            Functions &&... funcs ) const {
			return m_data->fork_join( std::forward<Function>( joiner ),
			                          std::forward<Functions>( funcs )... );
		}
	};
	// future_result_t

	template<>
	struct future_result_t<void> : public impl::future_result_base_t {
		using result_type_t = void;
		using result_t = daw::expected_t<result_type_t>;
		using m_data_t = impl::member_data_t<result_type_t>;
		std::shared_ptr<m_data_t> m_data;

	public:
		future_result_t( );
		explicit future_result_t( task_scheduler ts );
		explicit future_result_t( daw::shared_latch sem,
		                          task_scheduler ts = get_task_scheduler( ) );

		std::weak_ptr<m_data_t> weak_ptr( );
		void wait( ) const override;

		template<typename Rep, typename Period>
		future_status
		wait_for( std::chrono::duration<Rep, Period> rel_time ) const {
			return m_data->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		future_status
		wait_until( std::chrono::time_point<Clock, Duration> timeout_time ) const {
			return m_data->wait_until( timeout_time );
		}

		void get( ) const;
		bool try_wait( ) const override;
		explicit operator bool( ) const;
		bool is_exception( ) const;

		void set_value( );
		void set_exception( );
		void set_exception( std::exception_ptr ptr );

		template<typename Exception>
		void set_exception( Exception const &ex ) {
			m_data->set_exception( std::make_exception_ptr( ex ) );
		}

		template<typename Function, typename... Args>
		void from_code( Function &&func, Args &&... args ) {
			m_data->from_code(
			  daw::make_void_function( std::forward<Function>( func ) ),
			  std::forward<Args>( args )... );
		}

		template<typename Function>
		decltype( auto ) next( Function &&function ) const {
			return m_data->next( std::forward<Function>( function ) );
		}

		template<typename... Functions>
		decltype( auto ) fork( Functions &&... funcs ) const {
			return m_data->fork( std::forward<Functions>( funcs )... );
		}
	}; // future_result_t<void>

	template<typename result_t, typename Function>
	constexpr decltype( auto ) operator|( future_result_t<result_t> &lhs,
	                                      Function &&rhs ) {
		static_assert( traits::is_callable_v<Function, result_t>,
		               "Supplied function must be callable with result of future" );
		return lhs.next( std::forward<Function>( rhs ) );
	}

	template<typename result_t, typename Function>
	constexpr decltype( auto ) operator|( future_result_t<result_t> const &lhs,
	                                      Function &&rhs ) {
		static_assert( traits::is_callable_v<Function, result_t>,
		               "Supplied function must be callable with result of future" );
		return lhs.next( std::forward<Function>( rhs ) );
	}

	template<typename result_t, typename Function>
	constexpr decltype( auto ) operator|( future_result_t<result_t> &&lhs,
	                                      Function &&rhs ) {
		static_assert( traits::is_callable_v<Function, result_t>,
		               "Supplied function must be callable with result of future" );
		return lhs.next( std::forward<Function>( rhs ) );
	}

	template<typename Function, typename... Args,
	         std::enable_if_t<traits::is_callable_v<Function, Args...>,
	                          std::nullptr_t> = nullptr>
	auto make_future_result( task_scheduler ts, Function &&func,
	                         Args &&... args ) {
		using result_t =
		  std::decay_t<decltype( func( std::forward<Args>( args )... ) )>;
		auto result = future_result_t<result_t>( );

		ts.add_task( [result, func = std::forward<Function>( func ),
		              args = std::make_tuple(
		                std::forward<Args>( args )... )]( ) mutable -> void {
			result.from_code(
			  [func = std::move( func ), args = std::move( args )]( ) mutable {
				  return daw::apply( std::move( func ), std::move( args ) );
			  } );
		} );
		return result;
	}

	namespace impl {
		template<typename Result, typename Function, typename... Args>
		struct future_task_t {
			Result result;
			Function func;
			std::tuple<Args...> args;

			template<typename R, typename F, typename... A>
			constexpr future_task_t( R &&r, F &&f, A &&... a )
			  : result( std::forward<R>( r ) )
			  , func( std::forward<F>( f ) )
			  , args( std::forward<A>( a )... ) {}

			void operator( )( ) {
				try {
					result.set_value(
					  daw::apply( std::move( func ), std::move( args ) ) );
				} catch( ... ) { result.set_exception( ); }
			}
		};

		template<typename Result, typename Function, typename... Args>
		future_task_t<Result, Function, Args...> constexpr make_future_task(
		  Result &&result, Function &&func, Args &&... args ) {
			return future_task_t<Result, Function, Args...>(
			  std::forward<Result>( result ), std::forward<Function>( func ),
			  std::forward<Args>( args )... );
		}
	} // namespace impl

	template<typename Function, typename... Args,
	         std::enable_if_t<traits::is_callable_v<Function, Args...>,
	                          std::nullptr_t> = nullptr>
	auto make_future_result( task_scheduler ts, daw::shared_latch sem,
	                         Function &&func, Args &&... args ) {
		using result_t =
		  std::decay_t<decltype( func( std::forward<Args>( args )... ) )>;
		auto result = future_result_t<result_t>( std::move( sem ) );
		ts.add_task( impl::make_future_task( result, std::forward<Function>( func ),
		                                     std::forward<Args>( args )... ) );

		return result;
	} // namespace daw

	template<typename Function, typename... Args,
	         std::enable_if_t<traits::is_callable_v<Function, Args...>,
	                          std::nullptr_t> = nullptr>
	decltype( auto ) make_future_result( Function &&func, Args &&... args ) {
		return make_future_result( get_task_scheduler( ),
		                           std::forward<Function>( func ),
		                           std::forward<Args>( args )... );
	}

	template<typename... Args>
	decltype( auto ) async( Args &&... args ) {
		return make_future_result( std::forward<Args>( args )... );
	}

	template<typename... Functions>
	decltype( auto )
	make_callable_future_result_group( Functions &&... functions ) {
		return impl::future_group_result_t<Functions...>(
		  std::forward<Functions>( functions )... );
	}

	/// Create a group of functions that all return at the same time.  A tuple of
	/// results is returned
	//
	//  @param functions a list of functions of form Result( )
	template<typename... Functions>
	constexpr decltype( auto )
	make_future_result_group( Functions &&... functions ) {
		return make_callable_future_result_group(
		  std::forward<Functions>( functions )... )( );
	}

	std::false_type is_future_result_impl( ... );

	template<typename T>
	constexpr std::true_type is_future_result_impl( future_result_t<T> const & );

	template<typename T>
	constexpr bool is_future_result_v =
	  decltype( is_future_result_impl( std::declval<T>( ) ) )::value;

	namespace impl {
		template<typename Iterator, typename OutputIterator, typename BinaryOp>
		void reduce_futures2( Iterator first, Iterator last,
		                      OutputIterator out_first, BinaryOp &&binary_op ) {
			auto const sz = std::distance( first, last );
			if( sz == 0 ) {
				return;
			} else if( sz == 1 ) {
				*out_first++ = *first++;
				return;
			}
			bool const odd_count = sz % 2 == 1;
			if( odd_count ) {
				last = std::next( first, sz - 1 );
			}
			while( first != last ) {
				auto l = *first++;
				auto r_it = first++;
				*out_first++ = l.next( [r = *r_it, binary_op]( auto &&result ) mutable {
					return binary_op( std::forward<decltype( result )>( result ),
					                  r.get( ) );
				} );
			}
			if( odd_count ) {
				*out_first++ = *last;
			}
		}
	} // namespace impl

	template<
	  typename RandomIterator, typename RandomIterator2, typename BinaryOperation,
	  typename ResultType = future_result_t<
	    std::decay_t<decltype( ( *std::declval<RandomIterator>( ) ).get( ) )>>>
	ResultType reduce_futures( RandomIterator first, RandomIterator2 last,
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
		constexpr decltype( auto ) future_apply_impl( F &&f, Tuple &&t,
		                                              std::index_sequence<I...> ) {
			return daw::invoke( std::forward<F>( f ),
			                    std::get<I>( std::forward<Tuple>( t ) ).get( )... );
		}
	} // namespace impl

	template<typename F, typename Tuple>
	decltype( auto ) future_apply( F &&f, Tuple &&t ) {
		return impl::future_apply_impl(
		  std::forward<F>( f ), std::forward<Tuple>( t ),
		  std::make_index_sequence<daw::tuple_size_v<std::decay_t<Tuple>>>{} );
	}

	template<typename TPFutureResults, typename Function,
	         std::enable_if_t<daw::traits::is_tuple_v<TPFutureResults>,
	                          std::nullptr_t> = nullptr>
	decltype( auto ) join( TPFutureResults &&results, Function &&next_function ) {
		auto result = make_future_result(
		  [results = std::forward<TPFutureResults>( results ),
		   next_function = std::forward<Function>( next_function )]( ) mutable {
			  return future_apply( next_function, results );
		  } );
		return result;
	}
} // namespace daw
