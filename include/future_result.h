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
#include <daw/daw_semaphore.h>
#include <daw/daw_traits.h>

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
		  : m_data{std::make_shared<m_data_t>( get_task_scheduler( ) )} {}

		explicit future_result_t( task_scheduler ts )
		  : m_data{std::make_shared<m_data_t>( std::move( ts ) )} {}

		explicit future_result_t( daw::shared_semaphore sem,
		                          task_scheduler ts = get_task_scheduler( ) )
		  : m_data{
		      std::make_shared<m_data_t>( std::move( sem ), std::move( ts ) )} {}

		std::weak_ptr<m_data_t> weak_ptr( ) {
			return m_data;
		}

		void wait( ) const override {
			m_data->wait( );
		}

		template<typename Rep, typename Period>
		future_status
		wait_for( std::chrono::duration<Rep, Period> const &rel_time ) const {
			return m_data->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		future_status wait_until(
		  std::chrono::time_point<Clock, Duration> const &timeout_time ) const {
			return m_data->wait_until( timeout_time );
		}

		bool try_wait( ) const override {
			return m_data->try_wait( );
		}

		explicit operator bool( ) const {
			return m_data->try_wait( );
		}

		template<typename R>
		void set_value( R &&value ) noexcept {
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

		template<typename Function, typename... Args>
		void from_code( Function &&func, Args &&... args ) {
			static_assert(
			  daw::is_convertible_v<daw::traits::result_of_t<Function, Args...>,
			                        Result>,
			  "Function func with Args does not return a value that is "
			  "convertible to Result. e.g Result "
			  "r = func( args... ) must be valid" );
			m_data->from_code( std::forward<Function>( func ),
			                   std::forward<Args>( args )... );
		}

		bool is_exception( ) const {
			m_data->wait( );
			// TODO: look into not throwing and allowing values to be retrieved
			daw::exception::daw_throw_on_true(
			  m_data->status( ) == future_status::continued,
			  "Attempt to use a future that has been continued" );
			return m_data->m_result.has_exception( );
		}

		decltype( auto ) get( ) const {
			m_data->wait( );
			// TODO: look into not throwing and allowing values to be retrieved
			daw::exception::daw_throw_on_true(
			  m_data->status( ) == future_status::continued,
			  "Attempt to use a future that has been continued" );
			return m_data->m_result.get( );
		}

		template<typename Function>
		decltype( auto ) next( Function &&func ) {
			return m_data->next( std::forward<Function>( func ) );
		}

		template<typename... Functions>
		decltype( auto ) split( Functions &&... funcs ) {
			return m_data->split( std::forward<Functions>( funcs )... );
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
		explicit future_result_t( daw::shared_semaphore sem,
		                          task_scheduler ts = get_task_scheduler( ) );

		std::weak_ptr<m_data_t> weak_ptr( );
		void wait( ) const override;

		template<typename Rep, typename Period>
		future_status
		wait_for( std::chrono::duration<Rep, Period> const &rel_time ) const {
			return m_data->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		future_status wait_until(
		  std::chrono::time_point<Clock, Duration> const &timeout_time ) const {
			return m_data->wait_until( timeout_time );
		}

		void get( ) const;
		bool try_wait( ) const override;
		explicit operator bool( ) const;
		bool is_exception( ) const;

		void set_value( ) noexcept;
		void set_exception( );

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
		decltype( auto ) next( Function &&function ) {
			return m_data->next( std::forward<Function>( function ) );
		}

		template<typename... Functions>
		decltype( auto ) split( Functions &&... funcs ) {
			return m_data->split( std::forward<Functions>( funcs )... );
		}
	}; // future_result_t<void>

	template<typename result_t, typename Function>
	constexpr decltype( auto ) operator|( future_result_t<result_t> lhs,
	                                      Function &&rhs ) {
		return lhs.next( std::forward<Function>( rhs ) );
	}

	template<typename Function, typename... Args>
	auto make_future_result( task_scheduler ts, Function &&func,
	                         Args &&... args ) {
		using result_t =
		  std::decay_t<decltype( func( std::forward<Args>( args )... ) )>;
		auto result = future_result_t<result_t>{};
		ts.add_task( impl::convert_function_to_task(
		  result, std::forward<Function>( func ), std::forward<Args>( args )... ) );
		return result;
	}

	template<typename Function, typename... Args>
	auto make_future_result( task_scheduler ts, daw::shared_semaphore sem,
	                         Function &&func, Args &&... args ) {
		using result_t =
		  std::decay_t<decltype( func( std::forward<Args>( args )... ) )>;
		auto result = future_result_t<result_t>{std::move( sem )};

		ts.add_task( impl::convert_function_to_task(
		  result, std::forward<Function>( func ), std::forward<Args>( args )... ) );
		return result;
	}

	template<typename Function, typename... Args>
	decltype( auto ) make_future_result( Function &&func, Args &&... args ) {
		return make_future_result( get_task_scheduler( ),
		                           std::forward<Function>( func ),
		                           std::forward<Args>( args )... );
	}

	template<typename... Functions>
	decltype( auto )
	make_callable_future_result_group( Functions &&... functions ) {
		return impl::future_group_result_t<Functions...>{
		  std::forward<Functions>( functions )...};
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

	std::false_type is_future_result_impl( ... ) noexcept;

	template<typename T>
	constexpr std::true_type
	is_future_result_impl( future_result_t<T> const & ) noexcept;

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
				*out_first = *first++;
				return;
			}
			bool const odd_count = sz % 2 == 1;
			if( odd_count ) {
				last = std::next( first, sz - 1 );
			}
			while( first != last ) {
				auto l = first++;
				auto r_it = first++;
				*out_first = l->next( [r = *r_it, binary_op]( auto &&result ) mutable {
					return binary_op( std::forward<decltype( result )>( result ),
					                  r.get( ) );
				} );
			}
			if( odd_count ) {
				*out_first = *last;
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

		std::list<ResultType> results{};
		impl::reduce_futures2( first, last, std::back_inserter( results ),
		                       binary_op );

		while( results.size( ) > 1 ) {
			std::list<ResultType> tmp{};
			impl::reduce_futures2( first, last, std::back_inserter( tmp ),
			                       binary_op );
			results = tmp;
		}
		return results.front( );
	}
} // namespace daw
