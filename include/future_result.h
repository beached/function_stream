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

#include <daw/cpp_17.h>
#include <daw/daw_expected.h>
#include <daw/daw_semaphore.h>
#include <daw/daw_traits.h>

namespace daw {
	enum class future_status { ready, timeout, deferred };
	struct future_result_base_t {
		future_result_base_t( ) = default;
		future_result_base_t( future_result_base_t const & ) = default;
		future_result_base_t( future_result_base_t && ) noexcept = default;
		future_result_base_t &operator=( future_result_base_t const & ) = default;
		future_result_base_t &operator=( future_result_base_t && ) noexcept = default;

		virtual ~future_result_base_t( );
		virtual void wait( ) const = 0;
		virtual bool try_wait( ) const = 0;
		explicit operator bool( ) const;
	}; // future_result_base_t

	template<typename Result>
	struct future_result_t : public future_result_base_t {
		using result_type_t = std::decay_t<Result>;
		using result_t = daw::expected_t<result_type_t>;
		struct member_data_t {
			daw::semaphore m_semaphore;
			result_t m_result;
			future_status m_status;

			member_data_t( ) : m_result{}, m_status{future_status::deferred} {}

			~member_data_t( ) = default;

		  private:
			member_data_t( member_data_t const & ) = default;
			member_data_t &operator=( member_data_t const & ) = default;

			member_data_t( member_data_t &&other ) noexcept
			    : m_semaphore{std::move( other.m_semaphore )}
			    , m_result{std::move( other.m_result )}
			    , m_status{std::move( other.m_status )} {}

			member_data_t &operator=( member_data_t && rhs ) noexcept {
				m_semaphore = std::move( rhs.m_semaphore );
				m_result = std::move( rhs.m_result );
				m_status = std::move( rhs.m_status );
				return *this;
			}

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

		~future_result_t( ) override = default;
		future_result_t( future_result_t const & ) = default;
		future_result_t( future_result_t && ) noexcept = default;
		future_result_t &operator=( future_result_t const & ) = default;
		future_result_t &operator=( future_result_t && ) noexcept = default;

		std::weak_ptr<member_data_t> weak_ptr( ) {
			return m_data;
		}

		void wait( ) const override {
			m_data->m_semaphore.wait( );
		}

		template<typename... Args>
		future_status wait_for( Args &&... args ) const {
			if( future_status::deferred == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_for( std::forward<Args>( args )... ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		template<typename... Args>
		future_status wait_until( Args &&... args ) const {
			if( future_status::deferred == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_until( std::forward<Args>( args )... ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		Result const &get( ) const {
			wait( );
			return m_data->m_result.get( );
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

		bool is_exception( ) const {
			wait( );
			return m_data->m_result.has_exception( );
		}

		template<typename Function, typename... Args>
		void from_code( Function func, Args &&... args ) {
			m_data->from_code( std::move( func ), std::forward<Args>( args )... );
		}
	}; // future_result_t

	template<>
	struct future_result_t<void> : public future_result_base_t {
		using result_t = daw::expected_t<void>;
		struct member_data_t {
			daw::semaphore m_semaphore;
			result_t m_result;
			future_status m_status;

			member_data_t( );
			~member_data_t( );

			member_data_t( member_data_t const & ) =
			    delete; // TODO: investigate what member isn't copyable.  should be private
			member_data_t &operator=( member_data_t const & ) =
			    delete; // TODO: investigate what member isn't copyable. should be private
		  private:
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

		~future_result_t( ) override;
		future_result_t( future_result_t const & ) = default;
		future_result_t( future_result_t && ) noexcept = default;
		future_result_t &operator=( future_result_t const & ) = default;
		future_result_t &operator=( future_result_t && ) noexcept = default;

		std::weak_ptr<member_data_t> weak_ptr( );
		void wait( ) const override;

		template<typename... Args>
		future_status wait_for( Args &&... args ) const {
			if( future_status::deferred == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_for( std::forward<Args>( args )... ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		template<typename... Args>
		future_status wait_until( Args &&... args ) const {
			if( future_status::deferred == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_until( std::forward<Args>( args )... ) ) {
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

		bool is_exception( ) const;

		template<typename Function, typename... Args>
		void from_code( Function func, Args &&... args ) {
			m_data->from_code( daw::make_void_function( func ), std::forward<Args>( args )... );
		}
	}; // future_result_t

	namespace impl {
		template<typename Result, typename Function, typename... Args>
		struct f_caller_t {
			Result &m_result;
			Function m_function;
			std::tuple<Args...> m_args;
			f_caller_t( Result &result, Function func, Args &&... args )
			    : m_result{result}, m_function{std::move( func )}, m_args{std::forward<Args>( args )...} {}

			void operator( )( ) {
				m_result.from_code( [&]( ) { return daw::apply( m_function, m_args ); } );
			}
		}; // f_caller_t

		template<typename Result, typename Function, typename... Args>
		auto make_f_caller( Result &result, Function func, Args &&... args ) {
			return f_caller_t<Result, Function, Args...>{result, std::move( func ), std::forward<Args>( args )...};
		}
	} // namespace impl

	template<typename TaskScheduler, typename Function, typename... Args>
	auto make_future_result( TaskScheduler &ts, Function func, Args &&... args ) {
		using result_t = std::decay_t<decltype( func( std::forward<Args>( args )... ) )>;
		future_result_t<result_t> result;
		ts.add_task( impl::make_f_caller( result, func, std::forward<Args>( args )... ) );
		return result;
	}
} // namespace daw
