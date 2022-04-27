// The MIT License (MIT)
//
// Copyright (c) Darrell Wright
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

#include "impl/daw_function.h"
#include "impl/daw_latch.h"
#include "impl/future_result_impl.h"
#include "task_scheduler.h"

#include <daw/cpp_17.h>
#include <daw/daw_exception.h>
#include <daw/daw_expected.h>
#include <daw/daw_mutable_capture.h>
#include <daw/daw_traits.h>
#include <daw/vector.h>

#include <chrono>
#include <list>
#include <memory>
#include <tuple>
#include <utility>

namespace daw {
	template<typename Result>
	struct [[nodiscard]] future_result_t : impl::future_result_base_t {
		using result_type_t = daw::remove_cvref_t<Result>;
		using result_t = daw::expected_t<result_type_t>;
		using m_data_t = impl::member_data_t<result_type_t>;

	private:
		m_data_t m_data = m_data_t( get_task_scheduler( ) );

		explicit future_result_t( m_data_t d ) noexcept
		  : m_data( DAW_MOVE( d ) ) {}

	public:
		future_result_t( ) = default;

		explicit future_result_t( task_scheduler ts )
		  : m_data( DAW_MOVE( ts ) ) {}

		future_result_t( daw::shared_cnt_sem sem, task_scheduler ts )
		  : m_data( DAW_MOVE( sem ), DAW_MOVE( ts ) ) {}

		explicit future_result_t( daw::shared_cnt_sem sem )
		  : m_data( DAW_MOVE( sem ), get_task_scheduler( ) ) {}

	public:
		[[nodiscard]] auto get_handle( ) const {
			using data_handle_t = daw::remove_cvref_t<decltype( m_data.get_handle( ) )>;

			class handle_t {
				data_handle_t m_handle;

				explicit handle_t( data_handle_t hnd )
				  : m_handle( DAW_MOVE( hnd ) ) {}

				friend future_result_t<Result>;

			public:
				using type = result_type_t;

				[[nodiscard]] bool expired( ) const {
					return m_handle.expired( );
				}

				[[nodiscard]] std::optional<future_result_t> lock( ) const {
					if( auto lck = m_handle.lock( ); lck ) {
						return future_result_t( DAW_MOVE( *lck ) );
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
		[[nodiscard]] future_status wait_for( std::chrono::duration<Rep, Period> rel_time ) const {
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
			m_data.set_value( DAW_FWD( value ) );
		}

		template<typename Exception>
		void set_exception( Exception &&ex ) {
			m_data.set_exception( std::make_exception_ptr( DAW_FWD( ex ) ) );
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
			  std::is_convertible_v<decltype( DAW_FWD( func )( DAW_FWD( args )... ) ), Result>,
			  "Function func with Args does not return a value that is "
			  "convertible to Result. e.g Result "
			  "r = func( args... ) must be valid" );
			m_data.from_code( fs::impl::make_callable( DAW_FWD( func ) ), DAW_FWD( args )... );
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
			return m_data.next( fs::impl::make_callable( DAW_FWD( func ) ) );
		}

		template<typename... Functions>
		[[nodiscard]] decltype( auto ) fork( Functions &&...funcs ) {
			return m_data.fork( fs::impl::make_callable( DAW_FWD( funcs ) )... );
		}

		template<typename Function, typename... Functions>
		[[nodiscard]] decltype( auto ) fork_join( Function &&joiner, Functions &&...funcs ) {
			return m_data.fork_join( fs::impl::make_callable( DAW_FWD( joiner ) ),
			                         fs::impl::make_callable( DAW_FWD( funcs ) )... );
		}
	};
	// future_result_t

	template<>
	struct [[nodiscard]] future_result_t<void> : public impl::future_result_base_t {
		using result_type_t = void;
		using result_t = daw::expected_t<result_type_t>;
		using m_data_t = impl::member_data_t<result_type_t>;

	private:
		m_data_t m_data = m_data_t( get_task_scheduler( ) );

		explicit future_result_t( m_data_t d ) noexcept
		  : m_data( DAW_MOVE( d ) ) {}

	public:
		future_result_t( ) = default;
		explicit future_result_t( task_scheduler ts );
		explicit future_result_t( daw::shared_cnt_sem sem, task_scheduler ts = get_task_scheduler( ) );

		[[nodiscard]] auto get_handle( ) const {
			using data_handle_t = daw::remove_cvref_t<decltype( m_data.get_handle( ) )>;
			class handle_t {
				data_handle_t m_handle;

				explicit handle_t( data_handle_t hnd )
				  : m_handle( DAW_MOVE( hnd ) ) {}

				friend future_result_t;

			public:
				using type = void;

				[[nodiscard]] bool expired( ) const {
					return m_handle.expired( );
				}

				[[nodiscard]] std::optional<future_result_t> lock( ) const {
					if( auto lck = m_handle.lock( ); lck ) {
						return future_result_t( DAW_MOVE( *lck ) );
					}
					return { };
				}
			};

			return handle_t( m_data.get_handle( ) );
		}

		void wait( ) const override;

		template<typename Rep, typename Period>
		[[nodiscard]] future_status wait_for( std::chrono::duration<Rep, Period> rel_time ) const {
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
			m_data.set_exception( std::make_exception_ptr( DAW_FWD( ex ) ) );
		}

		template<typename Function, typename... Args>
		void from_code( Function &&func, Args &&...args ) {
			m_data.from_code( daw::make_void_function( fs::impl::make_callable( DAW_FWD( func ) ) ),
			                  DAW_FWD( args )... );
		}

		template<typename Function>
		[[nodiscard]] decltype( auto ) next( Function &&function ) {
			return m_data.next( fs::impl::make_callable( DAW_FWD( function ) ) );
		}

		template<typename Function, typename... Functions>
		[[nodiscard]] decltype( auto ) fork( Function &&func, Functions &&...funcs ) const {
			return m_data.fork( fs::impl::make_callable( DAW_FWD( func ), DAW_FWD( funcs ) )... );
		}
	}; // future_result_t<void>

	template<typename T>
	future_result_t( T ) -> future_result_t<T>;

	template<typename result_t, typename Function>
	[[nodiscard]] constexpr decltype( auto ) operator|( future_result_t<result_t> &lhs,
	                                                    Function &&rhs ) {
		static_assert( daw::traits::is_callable_v<Function, result_t>,
		               "Supplied function must be callable with result of future" );
		return lhs.next( fs::impl::make_callable( DAW_FWD( rhs ) ) );
	}

	template<typename result_t, typename Function>
	[[nodiscard]] constexpr decltype( auto ) operator|( future_result_t<result_t> &&lhs,
	                                                    Function &&rhs ) {
		static_assert( daw::traits::is_callable_v<std::remove_reference_t<Function>, result_t>,
		               "Supplied function must be callable with result of future" );
		return lhs.next( fs::impl::make_callable( DAW_FWD( rhs ) ) );
	}

	template<typename Function, typename... Args>
	requires( invocable<Function, Args...> ) //
	  [[nodiscard]] auto make_future_result( task_scheduler ts, Function &&func, Args &&...args ) {
		using result_t = daw::remove_cvref_t<decltype( func( DAW_FWD( args )... ) )>;
		auto result = future_result_t<result_t>( );

		if( not ts.add_task( [result = daw::mutable_capture( result ),
		                      func = daw::mutable_capture( fs::impl::make_callable( DAW_FWD( func ) ) ),
		                      args = daw::mutable_capture( std::make_tuple( DAW_FWD( args )... ) )] {
			    result->from_code( [func = daw::mutable_capture( func.move_out( ) ),
			                        args = daw::mutable_capture( args.move_out( ) )]( ) {
				    return std::apply( func.move_out( ), args.move_out( ) );
			    } );
		    } ) ) {
			throw daw::unable_to_add_task_exception{ };
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
			  : result( DAW_FWD( r ) )
			  , func( DAW_FWD( f ) )
			  , args( DAW_FWD( a )... ) {}

			void operator( )( ) {
				try {
					result.set_value( std::apply( DAW_MOVE( func ), DAW_MOVE( args ) ) );
				} catch( ... ) { result.set_exception( ); }
			}
		};

		template<typename Result, typename Function, typename... Args>
		[[nodiscard]] constexpr future_task_t<Result, Function, Args...>
		make_future_task( Result &&result, Function &&func, Args &&...args ) {
			return { DAW_FWD( result ), fs::impl::make_callable( DAW_FWD( func ) ), DAW_FWD( args )... };
		}
	} // namespace impl

	template<typename Function, typename... Args>
	[[nodiscard]] auto
	make_future_result( task_scheduler ts, daw::shared_cnt_sem sem, Function &&func, Args &&...args ) {

		static_assert( daw::traits::is_callable_v<std::remove_reference_t<Function>, Args...> );
		using result_t = decltype( DAW_FWD( func )( DAW_FWD( args )... ) );
		auto result = future_result_t<result_t>( DAW_MOVE( sem ) );
		ts.add_task( impl::make_future_task( result,
		                                     fs::impl::make_callable( DAW_FWD( func ) ),
		                                     DAW_FWD( args )... ) );

		return result;
	} // namespace daw

	template<typename Function, typename... Args>
	[[nodiscard]] decltype( auto ) make_future_result( Function &&func, Args &&...args ) {
		static_assert( daw::traits::is_callable_v<std::remove_reference_t<Function>, Args...> );
		return make_future_result( get_task_scheduler( ),
		                           fs::impl::make_callable( DAW_FWD( func ) ),
		                           DAW_FWD( args )... );
	}

	template<typename Function, typename... Args>
	[[nodiscard]] decltype( auto ) async( Function &&func, Args &&...args ) {
		static_assert( daw::traits::is_callable_v<std::remove_reference_t<Function>, Args...> );
		return make_future_result( DAW_FWD( func ), DAW_FWD( args )... );
	}

	namespace async_impl {
		template<typename... Functions>
		[[nodiscard]] decltype( auto )
		make_callable_future_result_group_impl( Functions &&...functions ) {
			return impl::future_group_result_t<daw::remove_cvref_t<Functions>...>(
			  DAW_FWD( functions )... );
		}
	} // namespace async_impl

	template<typename... Functions>
	[[nodiscard]] decltype( auto ) make_callable_future_result_group( Functions &&...functions ) {
		return async_impl::make_callable_future_result_group_impl(
		  fs::impl::make_callable( DAW_FWD( functions ) )... );
	}

	/// Create a group of functions that all return at the same time.  A tuple of
	/// results is returned
	//
	//  @param functions a list of functions of form Result( )
	template<typename... Functions>
	[[nodiscard]] decltype( auto ) make_future_result_group( Functions &&...functions ) {
		return make_callable_future_result_group(
		  fs::impl::make_callable( DAW_FWD( functions ) )... )( );
	}

	template<typename T>
	concept FutureResult = requires {
		typename T::i_am_a_future_result;
	};

	namespace impl {
		template<input_or_output_iterator Iterator, typename OutputIterator, typename BinaryOp>
		OutputIterator reduce_futures2( Iterator first,
		                                Iterator last,
		                                OutputIterator out_it,
		                                BinaryOp const &binary_op ) {
			auto const sz = std::distance( first, last );
			assert( sz >= 0 );
			if( sz == 0 ) {
				return out_it;
			} else if( sz == 1 ) {
				*out_it = *first;
				++out_it;
				++first;
				return out_it;
			}
			bool const odd_count = sz % 2 == 1;
			if( odd_count ) {
				last = std::next( first, sz - 1 );
			}
			while( first != last ) {
				auto l_it = ++first;
				auto r_it = ++first;
				auto &lhs = *l_it;
				*out_it =
				  DAW_MOVE( lhs ).next( [rhs = daw::mutable_capture( DAW_MOVE( *r_it ) ),
				                         binary_op = daw::mutable_capture( binary_op )]( auto &&result ) {
					  return binary_op.move_out( )( DAW_FWD( result ), rhs.move_out( ).get( ) );
				  } );
				++out_it;
			}
			if( odd_count ) {
				*out_it = DAW_MOVE( *last );
				++out_it;
			}
			return out_it;
		}
	} // namespace impl

	template<random_access_iterator RandomIterator,
	         random_access_iterator RandomIterator2,
	         typename BinaryOperation>
	[[nodiscard]] auto
	reduce_futures( RandomIterator first, RandomIterator2 last, BinaryOperation &&binary_op ) {
		using ResultType =
		  future_result_t<DAW_TYPEOF( ( std::declval<iter_value_type<RandomIterator>>( ) ).get( ) )>;
		static_assert( FutureResult<iter_value_type<RandomIterator>>,
		               "RandomIterator's value type must be a future result" );
		auto const rng_sz = static_cast<std::size_t>( std::distance( first, last ) );
		assert( rng_sz > 0 );
		auto results =
		  daw::vector<ResultType>( do_resize_and_overwrite,
		                           ( rng_sz / 2 ) + ( rng_sz % 2 ),
		                           [&]( ResultType *ptr, std::size_t sz ) {
			                           ResultType const *const last_out =
			                             impl::reduce_futures2( first, last, ptr, binary_op );
			                           auto const out_sz = static_cast<std::size_t>( last_out - ptr );
			                           assert( out_sz == sz );
			                           return sz;
		                           } );

		auto tmp = daw::vector<ResultType>( );
		while( results.size( ) > 1 ) {
			tmp.clear( );
			auto const next_sz = ( results.size( ) / 2 ) + ( results.size( ) % 2 );
			tmp.resize_and_overwrite( next_sz, [&]( ResultType *ptr, std::size_t sz ) {
				ResultType const *const last_out =
				  impl::reduce_futures2( results.begin( ), results.end( ), ptr, binary_op );
				auto const out_sz = static_cast<std::size_t>( last_out - ptr );
				assert( out_sz == sz );
				return sz;
			} );

			std::swap( results, tmp );
		}
		return results.front( );
	}

	namespace impl {
		template<typename F, typename Tuple, std::size_t... I>
		[[nodiscard]] constexpr decltype( auto )
		future_apply_impl( F &&f, Tuple &&t, std::index_sequence<I...> ) {
			return DAW_FWD( f )( std::get<I>( DAW_FWD( t ) ).get( )... );
		}
	} // namespace impl

	template<typename F, typename Tuple>
	decltype( auto ) future_apply( F &&f, Tuple &&t ) {
		return impl::future_apply_impl(
		  DAW_FWD( f ),
		  DAW_FWD( t ),
		  std::make_index_sequence<daw::tuple_size_v<daw::remove_cvref_t<Tuple>>>{ } );
	}

	template<typename TPFutureResults,
	         typename Function>
	requires( daw::traits::is_tuple_v<TPFutureResults> ) //
	  [[nodiscard]] auto join( TPFutureResults &&results, Function &&next_function ) {
		auto result = make_future_result( [results = daw::mutable_capture( DAW_FWD( results ) ),
		                                   next_function = daw::mutable_capture(
		                                     fs::impl::make_callable( DAW_FWD( next_function ) ) )]( ) {
			return future_apply( next_function.move_out( ), results.move_out( ) );
		} );
		return result;
	}
} // namespace daw
