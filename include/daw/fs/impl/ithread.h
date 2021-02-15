// The MIT License (MIT)
//
// Copyright (c) 2019 Darrell Wright
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

#include "daw_latch.h"

#include <daw/daw_move.h>
#include <daw/daw_utility.h>

#include <memory>
#include <thread>
#include <type_traits>

namespace daw::parallel {
	class interrupt_token_owner;

	class interrupt_token {
		::daw::latch const *m_condition;

		friend class ::daw::parallel::interrupt_token_owner;

		explicit interrupt_token( ::daw::latch const &cond ) noexcept
		  : m_condition( &cond ) {}

	public:
		[[nodiscard]] bool can_continue( ) const {
			return m_condition->try_wait( );
		}

		[[nodiscard]] explicit operator bool( ) const {
			return m_condition->try_wait( );
		}

		void wait( ) const {
			m_condition->wait( );
		}
	};

	class interrupt_token_owner {
		::daw::latch m_condition = ::daw::latch( 1 );

	public:
		interrupt_token_owner( ) = default;

		interrupt_token get_interrupt_token( ) const noexcept {
			return interrupt_token( m_condition );
		}

		void stop( ) {
			m_condition.notify( );
		}
	};

	class ithread {
		interrupt_token_owner m_continue{ };
		::daw::latch m_sem = ::daw::latch( 1 );
		::std::thread m_thread;

	public:
		using id = ::std::thread::id;

		template<
		  typename Callable, typename... Args,
		  std::enable_if_t<std::is_invocable_v<Callable, interrupt_token, Args...>,
		                   std::nullptr_t> = nullptr>
		explicit ithread( Callable &&callable, Args &&...args )
		  : m_thread( DAW_FWD( callable ), m_continue.get_interrupt_token( ),
		              DAW_FWD( args )... ) {

			m_thread.detach( );
		}

		template<typename Callable, typename... Args,
		         ::std::enable_if_t<
		           ::daw::all_true_v<
		             not ::std::is_invocable_v<Callable, interrupt_token, Args...>,
		             ::std::is_invocable_v<Callable, Args...>>,
		           ::std::nullptr_t> = nullptr>
		explicit ithread( Callable &&callable, Args &&...args )
		  : m_continue( )
		  , m_thread(
		      [this, callable = mutable_capture( DAW_FWD( callable ) )](
		        auto &&...lambda_args ) {
			      auto const on_exit =
			        ::daw::on_scope_exit( [&] { m_sem.notify( ); } );
			      auto &func = *callable;
			      return func( DAW_FWD( lambda_args )... );
		      },
		      DAW_FWD( args )... ) {}

		ithread( ithread && ) = delete;
		ithread( ithread const & ) = delete;
		ithread &operator=( ithread && ) = delete;
		ithread &operator=( ithread const & ) = delete;

		inline ~ithread( ) {
			try {
				m_continue.stop( );
				if( m_thread.joinable( ) ) {
					m_thread.join( );
				}
				m_sem.wait( );
			} catch( ... ) {
				// Do not let an exception take us down
			}
		}

		[[nodiscard]] inline bool joinable( ) const {
			return m_sem.try_wait( );
		}

		[[nodiscard]] inline std::thread::id get_id( ) const noexcept {
			return m_thread.get_id( );
		}

		[[nodiscard]] inline static unsigned hardware_concurrency( ) noexcept {
			return std::thread::hardware_concurrency( );
		}

		inline void stop( ) {
			m_continue.stop( );
		}

		inline void join( ) {
			m_sem.wait( );
		}

		inline void stop_and_wait( ) {
			stop( );
			join( );
		}

		inline void detach( ) {
			m_sem.reset( 0 );
		}
	};
} // namespace daw::parallel
