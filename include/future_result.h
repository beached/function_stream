// The MIT License (MIT)
//
// Copyright (c) 2016 Darrell Wright
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

#include <daw/daw_expected.h>
#include <daw/daw_semaphore.h>

namespace daw {
	enum class future_status { ready, timeout, deferred };
	struct future_result_base_t {
		future_result_base_t( ) = default;
		future_result_base_t( future_result_base_t const & ) = default;
		future_result_base_t( future_result_base_t && ) = default;
		future_result_base_t & operator=( future_result_base_t const & ) = default;
		future_result_base_t & operator=( future_result_base_t && ) = default;
		
		virtual ~future_result_base_t( );
		virtual void wait( ) const = 0;
		virtual bool try_wait( ) const = 0;
		explicit operator bool( ) const;
	};	// future_result_base_t

	template<typename Result>
	struct future_result_t: public future_result_base_t {
		struct member_data_t {
			daw::semaphore m_semaphore;
			using result_t = daw::expected_t<Result>;
			result_t m_result;
			future_status m_status;

			member_data_t( ):
				m_semaphore { },
				m_result { },
				m_status { future_status::deferred } { }

			~member_data_t( ) = default;
		private:
			member_data_t( member_data_t const & ) = default;
			member_data_t( member_data_t && ) = default;
			member_data_t & operator=( member_data_t const & ) = default;
			member_data_t & operator=( member_data_t && ) = default;
		public:
			void set_value( Result value ) noexcept {
				m_result = std::move( value );
				m_status = future_status::ready;
				m_semaphore.notify( );
			}

			void set_value( member_data_t & other ) {
				m_result = std::move( other.m_result );
				m_status = std::move( other.m_status );
				m_semaphore.notify( );
			}

			template<typename Function, typename... Args>
			void from_code( Function && func, Args&&... args ) {
				m_result = result_t{ std::forward<Function>( func ), std::forward<Args>( args )... };
				m_status = future_status::ready;
				m_semaphore.notify( );
			}

			void from_exception( std::exception_ptr ptr ) {
				m_result = std::move( ptr );
				m_status = future_status::ready;
				m_semaphore.notify( );
			}
		};	// member_data_t

		std::shared_ptr<member_data_t> m_data;

	public:
		future_result_t( ):
			m_data { std::make_shared<member_data_t>( ) } { }

		~future_result_t( ) = default;
		future_result_t( future_result_t const & ) = default;
		future_result_t( future_result_t && ) = default;
		future_result_t & operator=( future_result_t const & ) = default;
		future_result_t & operator=( future_result_t && ) = default;

		std::weak_ptr<member_data_t> weak_ptr( ) {
			return m_data;
		}

		void wait( ) const override {
			m_data->m_semaphore.wait( );
		}

		template<typename... Args>
		future_status wait_for( Args&&... args ) const {
			if( future_status::deferred == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_for( std::forward<Args>( args )... ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		template<typename... Args>
		future_status wait_until( Args&&... args ) const {
			if( future_status::deferred == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_until( std::forward<Args>( args )... ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		Result const & get( ) const {
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
		void set_exception( Exception const & ex ) {
			m_data->m_result.from_exception( ex );
			m_data->m_status = future_status::ready;
			m_data->m_semaphore.notify( );
		}

		bool is_exception( ) const {
			wait( );
			return m_data->m_result.has_exception( );
		}

		template<typename Function, typename... Args>
		void from_code( Function && func, Args&&... args ) {
			m_data->from_code( std::forward<Function>( func ), std::forward<Args>( args )... );
		}
	};	// future_result_t

	template<>
	struct future_result_t<void>: public future_result_base_t {
		struct member_data_t {
			daw::semaphore m_semaphore;
			daw::expected_t<void> m_result;
			future_status m_status;

			member_data_t( ):
				m_semaphore { },
				m_result { },
				m_status { future_status::deferred } { }

			~member_data_t( ) = default;
		private:
			member_data_t( member_data_t const & ) = default;
			member_data_t( member_data_t && ) = default;
			member_data_t & operator=( member_data_t const & ) = default;
			member_data_t & operator=( member_data_t && ) = default;
		public:
			void set_value( void ) noexcept {
				m_result = true;
				m_status = future_status::ready;
				m_semaphore.notify( );
			}

			void set_value( member_data_t & other ) {
				m_result = std::move( other.m_result );
				m_status = std::move( other.m_status );
				m_semaphore.notify( );
			}

			template<typename Function, typename... Args>
			void from_code( Function && func, Args&&... args ) {
				m_result = m_result.from_code( std::forward<Function>( func ), std::forward<Args>( args )... );
				m_status = future_status::ready;
				m_semaphore.notify( );
			}

			void from_exception( std::exception_ptr ptr ) {
				m_result = std::move( ptr );
				m_status = future_status::ready;
				m_semaphore.notify( );
			}
		};	// member_data_t

		std::shared_ptr<member_data_t> m_data;

	public:
		future_result_t( ):
			m_data { std::make_shared<member_data_t>( ) } { }

		~future_result_t( ) = default;
		future_result_t( future_result_t const & ) = default;
		future_result_t( future_result_t && ) = default;
		future_result_t & operator=( future_result_t const & ) = default;
		future_result_t & operator=( future_result_t && ) = default;

		std::weak_ptr<member_data_t> weak_ptr( ) {
			return m_data;
		}

		void wait( ) const override {
			m_data->m_semaphore.wait( );
		}

		template<typename... Args>
		future_status wait_for( Args&&... args ) const {
			if( future_status::deferred == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_for( std::forward<Args>( args )... ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		template<typename... Args>
		future_status wait_until( Args&&... args ) const {
			if( future_status::deferred == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_until( std::forward<Args>( args )... ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		void get( ) const {
			wait( );
			m_data->m_result.get( );
		}

		bool try_wait( ) const override {
			return m_data->m_semaphore.try_wait( );
		}

		explicit operator bool( ) const {
			return try_wait( );
		}

		void set_value( void ) noexcept {
			m_data->set_value( );
		}

		template<typename Exception>
		void set_exception( Exception const & ex ) {
			m_data->m_result.from_exception( ex );
			m_data->m_status = future_status::ready;
			m_data->m_semaphore.notify( );
		}

		bool is_exception( ) const {
			wait( );
			return m_data->m_result.has_exception( );
		}

		template<typename Function, typename... Args>
		void from_code( Function && func, Args&&... args ) {
			m_data->from_code( std::forward<Function>( func ), std::forward<Args>( args )... );
		}
	};	// future_result_t
}	// namespace daw
