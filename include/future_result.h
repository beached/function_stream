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

#include "task_scheduler.h"

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
			daw::shared_semaphore m_semaphore;
			result_t m_result;
			future_status m_status;

			member_data_t( ) : m_semaphore{}, m_result{}, m_status{future_status::deferred} {}

			member_data_t( daw::shared_semaphore semaphore )
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
		future_result_t( daw::shared_semaphore semaphore ) : m_data{std::move( semaphore )} {}

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

		template<typename... Args>
		future_status wait_for( Args &&... args ) const {
			if( future_status::deferred == m_data->m_status || future_status::ready == m_data->m_status ) {
				return m_data->m_status;
			}
			if( m_data->m_semaphore.wait_for( std::forward<Args>( args )... ) ) {
				return m_data->m_status;
			}
			return future_status::timeout;
		}

		template<typename... Args>
		future_status wait_until( Args &&... args ) const {
			if( future_status::deferred == m_data->m_status || future_status::ready == m_data->m_status ) {
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
			daw::shared_semaphore m_semaphore;
			result_t m_result;
			future_status m_status;

			member_data_t( );
			member_data_t( daw::shared_semaphore semaphore );

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
		future_result_t( daw::shared_semaphore semaphore );

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
	}; // future_result_t<void>

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

	namespace impl {
		template<size_t N, size_t SZ, typename... Callables>
		struct call_func_t {
			template<typename Results, typename... Args>
			void operator( )( daw::task_scheduler &ts, daw::shared_semaphore semaphore, Results &results,
			                  std::tuple<Callables...> const &callables, std::tuple<Args...> const & args ) {
				ts.add_task( [semaphore, &results, &callables, &args]( ) mutable {
					try {
						std::get<N>( results ) = daw::apply( std::get<N>( callables ), args );
					} catch( ... ) { std::get<N>( results ) = std::current_exception; }
					semaphore.notify( );
				} );
				call_func_t<N + 1, SZ, Callables...>{}( ts, semaphore, results, callables, args );
			}
		}; // call_func_t

		template<size_t SZ, typename... Callables>
		struct call_func_t<SZ, SZ, Callables...> {
			template<typename Results, typename... Args>
			constexpr void operator( )( daw::task_scheduler const &, daw::shared_semaphore const &, Results const &,
			                            std::tuple<Callables...> const &, std::tuple<Args...> const & ) noexcept {}
		}; // call_func_t<SZ, SZ, Callables..>

		template<typename Result, typename... Callables, typename... Args>
		void call_funcs( daw::task_scheduler &ts, daw::shared_semaphore semaphore, Result &result,
		                 std::tuple<Callables...> const &callables, std::tuple<Args...> args ) {
			call_func_t<0, sizeof...( Callables ), Callables...>{}( ts, semaphore, result, callables, args );
		}
	
		template<typename... Functions>
		class result_t {
			std::tuple<Functions...> tp_functions;
		public:
			result_t( Functions... fs ): tp_functions{std::make_tuple( std::move( fs )... )} { }

			template<typename... Args>
			auto operator()( Args... args ) {
				daw::shared_semaphore semaphore{1 - static_cast<intmax_t>( sizeof...( Functions ) )};
				using result_t = std::tuple<daw::expected_t<std::decay_t<decltype( std::declval<Functions>( )( args... ) )>>...>;
				auto tp_args = std::make_tuple( std::move( args )... );
				future_result_t<result_t> result;
				auto th = std::thread{
					[result, semaphore, tp_functions = std::move( tp_functions ), tp_args=std::move(tp_args)]( ) mutable noexcept {
						auto ts = get_task_scheduler( );
						result_t tp_result;
						impl::call_funcs( ts, semaphore, tp_result, tp_functions, tp_args );
						semaphore.wait( );
						result.set_value( std::move( tp_result ) );
					}
				};
				th.detach( );
				return result;
			}
		};	// result_t
	} // namespace impl

	/// Create a group of functions that all return at the same time.  A tuple of results is returned
	//
	//  @param functions a list of functions of form Result( )
	template<typename... Functions>
	auto make_future_result_group( Functions... functions ) {
		return impl::result_t<Functions...>{ std::move( functions )... }( );
	}

	template<typename... Functions>
	auto make_callable_future_result_group( Functions... functions ) {
		return impl::result_t<Functions...>{ std::move( functions )... };
	}
} // namespace daw

