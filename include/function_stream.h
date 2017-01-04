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
#include <utility>
#include <tuple>

#include <daw/daw_expected.h>

#include "task_scheduler.h"

namespace daw {
	namespace impl {
		template<typename Function, typename Tuple, size_t ...S>
		auto apply_tuple( Function func, Tuple && t, std::index_sequence<S...> ) {
			return func( std::forward<decltype(std::get<S>( t ))>( std::get<S>( t ) )... );
		}

		template<typename Function, typename Tuple, typename Index = std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>>
		auto apply_tuple( Function func, Tuple && t ) {
			return apply_tuple( func, std::forward<Tuple>( t ), Index { } );
		}

		template<size_t S, typename Tuple>
		using is_function_tag = std::integral_constant<bool, 0 <= S && S < std::tuple_size<std::decay_t<Tuple>>::value>;

		template<size_t S, typename Tuple>
		constexpr bool const is_function_tag_v = is_function_tag<S, Tuple>::value;

		template<size_t S, typename Tuple>
		using is_function_tag_t = typename is_function_tag<S, Tuple>::type;

		template<size_t pos, typename Package> struct call_task_t;
		template<typename Package> struct call_task_last_t;

		struct function_tag { using category = function_tag; };
		struct callback_tag { using category = callback_tag; };

		template<size_t S, typename Tuple>
		using which_type_t = typename std::conditional < S < std::tuple_size<std::decay_t<Tuple>>::value - 1, function_tag, callback_tag>::type;

		template<size_t pos, typename Package>
		void call_task( Package, callback_tag );
		template<size_t pos, typename Package>
		void call_task( Package, function_tag );

		template<size_t pos, typename Package>
		void call( Package package ) {
			get_task_scheduler( ).add_task( [p = std::move( package )]( ) {
				call_task<pos>( std::move( p ), typename impl::which_type_t<pos, decltype(p->f_list)>::category { } );
			} );
		}

		template<size_t pos, typename Package>
		void call_task( Package package, callback_tag ) {
			if( !package->continue_processing( ) ) {
				return;
			}
			auto const func = std::get<pos>( package->f_list );
			auto client_data = package->m_result.lock( );
			if( client_data ) {
				client_data->from_code( [&]( ) {
					return apply_tuple( func, std::move( package->targs ) );
				} );
			} else {
				apply_tuple( func, std::move( package->targs ) );
			}
		}

		template<size_t pos, typename Package>
		void call_task( Package package, function_tag ) {
			if( !package->continue_processing( ) ) {
				return;
			}
			auto const func = std::get<pos>( package->f_list );
			try {
				auto result = apply_tuple( func, std::move( package->targs ) );
				static size_t const new_pos = pos + 1;
				call<new_pos>( package->next_package( std::move( result ) ) );
			} catch( ... ) {
				auto result = package->m_result.lock( );
				if( result ) {
					result->from_exception( std::current_exception( ) );
				}
			}
		}
	}	// namespace impl 

	enum class future_status { ready, timeout, deferred };
	template<typename Result>
	struct future_result_t {
		struct member_data_t {
			daw::semaphore m_semaphore;
			daw::expected_t<Result> m_result;
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
				m_result.from_code( std::forward<Function>( func ), std::forward<Args>( args )... );
				m_status = future_status::ready;
				m_semaphore.notify( );
			}

			void from_exception( std::exception_ptr ptr ) {
				m_result.from_exception( std::move( ptr ) );
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

		void wait( ) const {
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

		Result & get( ) {
			wait( );
			return m_data->m_result.get( );
		}

		Result const & get( ) const {
			wait( );
			return m_data->m_result.get( );
		}

		explicit operator bool( ) const {
			return m_data->m_semaphore.try_wait( );
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

	namespace impl {
		template<typename R>
		constexpr auto weak_ptr_test( std::weak_ptr<R> wp ) { return static_cast<R*>(nullptr); }

		template<typename T>
		struct weak_ptr_type_impl {
			using type = decltype(*weak_ptr_test( std::declval<T>( ) ));
		};
		template<typename T>
		using weak_ptr_type_t = typename weak_ptr_type_impl<T>::type;


		template<typename Result, typename Functions, typename... Args>
		struct package_t {
			using functions_t = Functions;
			using arguments_t = std::tuple<Args...>;
			using result_t = Result;
			using result_value_t = weak_ptr_type_t<result_t>;

			functions_t f_list;
			arguments_t targs;
			result_t m_result;
			bool continue_on_result_destruction;

			package_t( ) = delete;
			~package_t( ) = default;
			package_t( package_t const & ) = delete;
			package_t( package_t && ) = default;
			package_t & operator=( package_t const & ) = delete;
			package_t & operator=( package_t && ) = default;

			bool destination_expired( ) const {
				return m_result.expired( );
			}

			bool continue_processing( ) const {
				return !destination_expired( ) || continue_on_result_destruction;
			}

			template<typename... NewArgs>
			auto next_package( NewArgs... nargs ) {
				return make_shared_package( continue_on_result_destruction, std::move( m_result ), std::move( f_list ), std::move( nargs )... );
			}

			package_t( bool continueonclientdestruction, result_t result, functions_t functions, Args... args ):
				f_list { std::move( functions ) },
				targs { std::make_tuple( std::move( args )... ) },
				m_result { result },
				continue_on_result_destruction { continueonclientdestruction } { }
		};	// package_t

		template<typename Result, typename Functions, typename... Args>
		auto make_package( bool continue_on_result_destruction, Result && result, Functions && functions, Args&&... args ) {
			return package_t<Result, Functions, Args...>{ continue_on_result_destruction, std::forward<Result>( result ), std::forward<Functions>( functions ), std::forward<Args>( args )... };
		}

		template<typename Result, typename Functions, typename... Args>
		auto make_shared_package( bool continue_on_result_destruction, Result && result, Functions && functions, Args&&... args ) {
			return std::make_shared<package_t<Result, Functions, Args...>>( make_package( continue_on_result_destruction, std::forward<Result>( result ), std::forward<Functions>( functions ), std::forward<Args>( args )... ) );
		}

		template<size_t pos, typename TFunctions, typename Arg>
		constexpr auto function_composer_impl( TFunctions const &, callback_tag, Arg arg ) {
			return std::move( arg );
		}

		template<size_t pos, typename TFunctions, typename Arg, typename = std::enable_if_t<!is_function_tag_v<pos, TFunctions>>>
		constexpr auto function_composer_impl( TFunctions const &, function_tag, Arg arg ) {
			return std::move( arg );
		}

		template<size_t pos, typename TFunctions, typename... Args, typename = std::enable_if_t<is_function_tag_v<pos, TFunctions>>>
		auto function_composer_impl( TFunctions const & funcs, function_tag, Args&&... args ) {
			auto result = std::get<pos>( funcs )(std::forward<Args>( args )...);
			return function_composer_impl<pos + 1>( funcs, typename which_type_t<pos, TFunctions>::category { }, std::move( result ) );
		}

		template<typename... Functions>
		struct function_composer_t {
			using tfunction_t = std::tuple<Functions...>;
			tfunction_t funcs;

			constexpr function_composer_t( Functions&&... fs ):
				funcs { std::make_tuple( fs... ) } { }

			template<typename... Args>
			auto apply( Args&&... args ) {
				return function_composer_impl<0>( funcs, typename which_type_t<0, tfunction_t>::category { }, std::forward<Args>( args )... );
			}
		};
	}	// namespace impl

	template<typename... Functions>
	class function_stream {
		static_assert(sizeof...(Functions) > 0, "Must pass more than 0 items");
		using function_t = std::tuple<std::decay_t<Functions>...>;
		using func_comp_t = impl::function_composer_t<Functions...>;

		function_t m_funcs;
	public:
		bool continue_on_result_destruction;

		constexpr function_stream( Functions... funcs ):
			m_funcs { std::make_tuple( std::move( funcs )... ) },
			continue_on_result_destruction { true } { }

		template<typename... Args>
		auto operator( )( Args... args ) const {
			using func_result_t = decltype(std::declval<func_comp_t>( ).apply( args... ));
			future_result_t<func_result_t> result;
			impl::call<0>( impl::make_shared_package( continue_on_result_destruction, result.weak_ptr( ), m_funcs, std::move( args )... ) );
			return result;
		}
	};	// function_stream

	template<typename... Functions>
	constexpr auto make_function_stream( Functions&&... funcs ) {
		return function_stream<Functions...>{ std::forward<Functions>( funcs )... };
	}

	template<typename T, typename... Ts>
	std::vector<T> create_vector( T && value, Ts&&... values ) {
		return std::vector<T>{ std::initializer_list<T>{ std::forward<T>( value ), std::forward<Ts>( values )...} };
	}
}    // namespace daw
