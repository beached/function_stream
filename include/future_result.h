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
#include <daw/daw_expected.h>
#include <daw/daw_semaphore.h>
#include <daw/daw_traits.h>

#include "task_scheduler.h"
#include "future_result_impl.h"

namespace daw {
	template<typename Result>
	struct future_result_t : public impl::future_result_base_t {
		using result_type_t = std::decay_t<Result>;
		using result_t = daw::expected_t<result_type_t>;

		struct member_data_t {
			daw::shared_semaphore m_semaphore;
			result_t m_result;
			future_status m_status;

			member_data_t( ) : m_semaphore{}, m_result{}, m_status{future_status::deferred} {}

			explicit member_data_t( daw::shared_semaphore semaphore )
			    : m_semaphore{std::move( semaphore )}, m_result{}, m_status{future_status::deferred} {}

			~member_data_t( ) = default;

		  private:
			member_data_t( member_data_t const & ) = default;
			member_data_t &operator=( member_data_t const & ) = default;
			member_data_t( member_data_t &&other ) noexcept = default;
			member_data_t &operator=( member_data_t &&rhs ) noexcept = default;

		  public:
			void set_value( Result value ) noexcept {
				m_result = std::move( value );
				m_status = future_status::ready;
				m_semaphore.notify( );
			}

			void set_value( member_data_t &other ) {
				m_result = std::move( other.m_result );
				m_status = std::move( other.m_status );
				m_semaphore.notify( );
			}

			template<typename Function, typename... Args>
			void from_code( Function func, Args &&... args ) {
				m_result = expected_from_code<result_type_t>( func, std::forward<Args>( args )... );
				m_status = future_status::ready;
				m_semaphore.notify( );
			}

			void from_exception( std::exception_ptr ptr ) {
				m_result = std::move( ptr );
				m_status = future_status::ready;
				m_semaphore.notify( );
			}
		}; // member_data_t

		std::shared_ptr<member_data_t> m_data;

	  public:
		future_result_t( ) : m_data{std::make_shared<member_data_t>( )} {}
		explicit future_result_t( daw::shared_semaphore semaphore ) : m_data{std::move( semaphore )} {}

		~future_result_t( ) override = default;
		future_result_t( future_result_t const & ) = default;
		future_result_t( future_result_t && ) noexcept = default;
		future_result_t &operator=( future_result_t const & ) = default;
		future_result_t &operator=( future_result_t && ) noexcept = default;

		std::weak_ptr<member_data_t> weak_ptr( ) {
			return m_data;
		}

		void wait( ) const override {
			if( m_data->m_status != future_status::ready ) {
				m_data->m_semaphore.wait( );
			}
		}

		template<typename Rep, typename Period>
        future_status wait_for( std::chrono::duration<Rep, Period> const &rel_time ) const {
			if( future_status::deferred == m_data->m_status || future_status::ready == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_for( rel_time ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		template<typename Clock, typename Duration>
        future_status wait_until( std::chrono::time_point<Clock, Duration> const & timeout_time ) const {
			if( future_status::deferred == m_data->m_status || future_status::ready == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_until( timeout_time ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		bool try_wait( ) const override {
			return m_data->m_semaphore.try_wait( );
		}

		explicit operator bool( ) const {
			return try_wait( );
		}

		void set_value( Result value ) noexcept {
			m_data->set_value( std::move( value ) );
		}

		template<typename Exception>
		void set_exception( Exception const &ex ) {
			m_data->m_result = result_t{typename result_t::exception_tag{}, ex};
			m_data->m_status = future_status::ready;
			m_data->m_semaphore.notify( );
		}

		void set_exception( ) {
			m_data->m_result = result_t{typename result_t::exception_tag{}};
			m_data->m_status = future_status::ready;
			m_data->m_semaphore.notify( );
		}

		template<typename Function, typename... Args>
		void from_code( Function func, Args &&... args ) {
			m_data->from_code( std::move( func ), std::forward<Args>( args )... );
		}

		bool is_exception( ) const {
			wait( );
			return m_data->m_result.has_exception( );
		}

		Result const &get( ) const {
			wait( );
			return m_data->m_result.get( );
		}
	}; // future_result_t

	template<>
	struct future_result_t<void> : public impl::future_result_base_t {
		using result_t = daw::expected_t<void>;
		struct member_data_t {
			daw::shared_semaphore m_semaphore;
			result_t m_result;
			future_status m_status;

			member_data_t( );
			explicit member_data_t( daw::shared_semaphore semaphore );

			~member_data_t( );

		  private:
			member_data_t( member_data_t const & ) = default;
			member_data_t &operator=( member_data_t const & ) = default;
			member_data_t( member_data_t && ) noexcept = default;
			member_data_t &operator=( member_data_t && ) noexcept = default;

		  public:
			void set_value( ) noexcept;
			void set_value( member_data_t &other );

			template<typename Function, typename... Args>
			void from_code( Function func, Args &&... args ) {
				m_result.from_code( func, std::forward<Args>( args )... );
				m_status = future_status::ready;
				m_semaphore.notify( );
			}

			void from_exception( std::exception_ptr ptr );
		}; // member_data_t

		std::shared_ptr<member_data_t> m_data;

	  public:
		future_result_t( );
		explicit future_result_t( daw::shared_semaphore semaphore );

		~future_result_t( ) override;
		future_result_t( future_result_t const & ) = default;
		future_result_t( future_result_t && ) noexcept = default;
		future_result_t &operator=( future_result_t const & ) = default;
		future_result_t &operator=( future_result_t && ) noexcept = default;

		std::weak_ptr<member_data_t> weak_ptr( );
		void wait( ) const override;

		template<typename Rep, typename Period>
        future_status wait_for( std::chrono::duration<Rep, Period> const &rel_time ) const {
			if( future_status::deferred == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_for( rel_time ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		template<typename Clock, typename Duration>
        future_status wait_until( std::chrono::time_point<Clock, Duration> const & timeout_time ) const {
			if( future_status::deferred == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_until( timeout_time ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		void get( ) const;
		bool try_wait( ) const override;
		explicit operator bool( ) const;
		void set_value( ) noexcept;

		template<typename Exception>
		void set_exception( Exception const &ex ) {
			m_data->m_result = result_t{typename result_t::exception_tag{}, ex};
			m_data->m_status = future_status::ready;
			m_data->m_semaphore.notify( );
		}

		void set_exception( ) {
			m_data->m_result = result_t{typename result_t::exception_tag{}};
			m_data->m_status = future_status::ready;
			m_data->m_semaphore.notify( );
		}

		bool is_exception( ) const;

		template<typename Function, typename... Args>
		void from_code( Function func, Args &&... args ) {
			m_data->from_code( daw::make_void_function( func ), std::forward<Args>( args )... );
		}
	}; // future_result_t<void>

	template<typename Function, typename... Args>
	auto make_future_result( task_scheduler ts, Function func, Args &&... args ) {
		using result_t = std::decay_t<decltype( func( std::forward<Args>( args )... ) )>;
		future_result_t<result_t> result;
		ts.add_task( impl::make_f_caller( result, func, std::forward<Args>( args )... ) );
		return result;
	}

	template<typename Function, typename... Args>
	auto make_future_result( task_scheduler ts, daw::shared_semaphore semaphore, Function func, Args &&... args ) {
		using result_t = std::decay_t<decltype( func( std::forward<Args>( args )... ) )>;
		future_result_t<result_t> result{std::move( semaphore )};
		ts.add_task( impl::make_f_caller( result, func, std::forward<Args>( args )... ) );
		return result;
	}

	template<typename Function, typename... Args>
	auto make_future_result( Function func, Args &&... args ) {
		return make_future_result( get_task_scheduler( ), func, std::forward<Args>( args )... );
	}

	template<typename... Functions>
	auto make_callable_future_result_group( Functions... functions ) {
		return impl::result_t<Functions...>{ std::move( functions )... };
	}

	/// Create a group of functions that all return at the same time.  A tuple of results is returned
	//
	//  @param functions a list of functions of form Result( )
	template<typename... Functions>
	auto make_future_result_group( Functions... functions ) {
		using result_t = decltype( make_callable_future_result_group( std::move( functions )... )( ) );
		boost::optional<result_t> result;
		blocking_section( [&]( ) { result = make_callable_future_result_group( std::move( functions )... )( ); } );
		return *result;
	}

} // namespace daw

