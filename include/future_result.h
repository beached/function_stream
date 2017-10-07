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

#include <chrono>
#include <memory>
#include <tuple>

#include <daw/cpp_17.h>
#include <daw/daw_exception.h>
#include <daw/daw_expected.h>
#include <daw/daw_semaphore.h>
#include <daw/daw_traits.h>

#include "future_result_impl.h"
#include "task_scheduler.h"

namespace daw {
	template<typename Result>
	struct future_result_t : public impl::future_result_base_t {
		using result_type_t = std::decay_t<Result>;
		using result_t = daw::expected_t<result_type_t>;
		using m_data_t = impl::member_data_t<result_type_t>;
		std::shared_ptr<m_data_t> m_data;

	public:
		future_result_t( task_scheduler ts = get_task_scheduler( ) )
		  : m_data{std::make_shared<m_data_t>( std::move( ts ) )} {}

		explicit future_result_t( daw::shared_semaphore sem, task_scheduler ts = get_task_scheduler( ) )
		  : m_data{std::make_shared<m_data_t>( std::move( sem ), std::move( ts ) )} {}

		~future_result_t( ) override = default;
		future_result_t( future_result_t const & ) = default;
		future_result_t( future_result_t && ) noexcept = default;
		future_result_t &operator=( future_result_t const & ) = default;
		future_result_t &operator=( future_result_t && ) noexcept = default;

		std::weak_ptr<m_data_t> weak_ptr( ) {
			return m_data;
		}

		void wait( ) const override {
			m_data->wait( );
		}

		template<typename Rep, typename Period>
		future_status wait_for( std::chrono::duration<Rep, Period> const &rel_time ) const {
			return m_data->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		future_status wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) const {
			return m_data->wait_until( timeout_time );
		}

		bool try_wait( ) const override {
			return m_data->try_wait( );
		}

		explicit operator bool( ) const {
			return m_data->try_wait( );
		}

		void set_value( Result value ) noexcept {
			m_data->set_value( std::move( value ) );
		}

		template<typename Exception>
		void set_exception( Exception const &ex ) {
			m_data->set_exception( std::make_exception_ptr( ex ) );
		}

		void set_exception( ) {
			m_data->set_exception( std::current_exception( ) );
		}

		template<typename Function, typename... Args>
		void from_code( Function func, Args &&... args ) {
			using func_result_t = decltype( func( std::forward<Args>( args )... ) );
			static_assert( std::is_convertible<func_result_t, Result>::value,
			               "Function func with Args does not return a value that is convertible to Result. e.g Result "
			               "r = func( args... ) must be valid" );
			m_data->from_code( std::move( func ), std::forward<Args>( args )... );
		}

		bool is_exception( ) const {
			m_data->wait( );
			// TODO: look into not throwing and allowing values to be retrieved
			daw::exception::daw_throw_on_true( m_data->status( ) == future_status::continued,
			                                   "Attempt to use a future that has been continued" );
			return m_data->m_result.has_exception( );
		}

		auto const &get( ) const {
			m_data->wait( );
			// TODO: look into not throwing and allowing values to be retrieved
			daw::exception::daw_throw_on_true( m_data->status( ) == future_status::continued,
			                                   "Attempt to use a future that has been continued" );
			return m_data->m_result.get( );
		}

		template<typename Function>
		auto next( Function &&next_function ) {
			return m_data->next( std::forward<Function>( next_function ) );
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
		future_result_t( task_scheduler ts = get_task_scheduler( ) );
		explicit future_result_t( daw::shared_semaphore sem, task_scheduler ts = get_task_scheduler( ) );

		~future_result_t( ) override;
		future_result_t( future_result_t const & ) = default;
		future_result_t( future_result_t && ) noexcept = default;
		future_result_t &operator=( future_result_t const & ) = default;
		future_result_t &operator=( future_result_t && ) noexcept = default;

		std::weak_ptr<m_data_t> weak_ptr( );
		void wait( ) const override;

		template<typename Rep, typename Period>
		future_status wait_for( std::chrono::duration<Rep, Period> const &rel_time ) const {
			return m_data->wait_for( rel_time );
		}

		template<typename Clock, typename Duration>
		future_status wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time ) const {
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
		void from_code( Function func, Args &&... args ) {
			m_data->from_code( daw::make_void_function( func ), std::forward<Args>( args )... );
		}

		template<typename Function>
		auto next( Function next_function ) {
			return m_data->next( next_function );
		}
	}; // future_result_t<void>

	template<typename result_t, typename NextFunction>
	auto operator>>( future_result_t<result_t> lhs, NextFunction next_func ) {
		return lhs.next( std::move( next_func ) );
	}

	template<typename Function, typename... Args>
	auto make_future_result( task_scheduler ts, Function func, Args &&... args ) {
		using result_t = std::decay_t<decltype( func( std::forward<Args>( args )... ) )>;
		future_result_t<result_t> result;
		ts.add_task( impl::convert_function_to_task( result, func, std::forward<Args>( args )... ) );
		return result;
	}

	template<typename Function, typename... Args>
	auto make_future_result( task_scheduler ts, daw::shared_semaphore sem, Function func, Args &&... args ) {
		using result_t = std::decay_t<decltype( func( std::forward<Args>( args )... ) )>;
		future_result_t<result_t> result{std::move( sem )};
		ts.add_task( impl::convert_function_to_task( result, func, std::forward<Args>( args )... ) );
		return result;
	}

	template<typename Function, typename... Args>
	auto make_future_result( Function func, Args &&... args ) {
		return make_future_result( get_task_scheduler( ), func, std::forward<Args>( args )... );
	}

	template<typename... Functions>
	auto make_callable_future_result_group( Functions... functions ) {
		return impl::future_group_result_t<Functions...>{std::move( functions )...};
	}

	/// Create a group of functions that all return at the same time.  A tuple of results is returned
	//
	//  @param functions a list of functions of form Result( )
	template<typename... Functions>
	auto make_future_result_group( Functions... functions ) {
		return make_callable_future_result_group( std::move( functions )... )( );
	}
} // namespace daw
