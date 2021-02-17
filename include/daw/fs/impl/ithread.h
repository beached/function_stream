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

#include <atomic>
#include <atomic_wait>
#include <memory>
#include <thread>
#include <type_traits>

namespace daw::parallel {
	class stop_token_owner;
	class ithread;

	class stop_token {
		std::weak_ptr<stop_token_owner> m_owner;

		friend class ::daw::parallel::stop_token_owner;

		inline explicit stop_token( std::weak_ptr<stop_token_owner> owner ) noexcept
		  : m_owner( daw::move( owner ) ) {}

	public:
		[[nodiscard]] inline bool can_continue( ) const;
		[[nodiscard]] inline explicit operator bool( ) const;
		inline void wait( ) const;
		inline void stop( );
	};

	class stop_token_owner
	  : public std::enable_shared_from_this<stop_token_owner> {
		std::atomic<std::ptrdiff_t> m_keep_going{ 1 };

	public:
		stop_token_owner( ) = default;

		inline stop_token get_interrupt_token( ) noexcept {
			return stop_token( this->shared_from_this( ) );
		}

		inline bool request_stop( ) {
			std::ptrdiff_t expected = 1;
			bool result = m_keep_going.compare_exchange_strong(
			  expected, 0, std::memory_order_release );
			std::atomic_notify_all( &m_keep_going );
			return result;
		}

		inline bool can_continue( ) const {
			return m_keep_going.load( std::memory_order_acquire ) == 1;
		}

		inline void wait( ) const {
			auto current = m_keep_going.load( std::memory_order_acquire );
			while( current != 0 ) {
				std::atomic_wait_explicit( &m_keep_going, current,
				                           std::memory_order_relaxed );
				current = m_keep_going.load( std::memory_order_acquire );
			}
		}
	};

	inline bool stop_token::can_continue( ) const {
		if( auto p = m_owner.lock( ); p ) {
			return p->can_continue( );
		} else {
			return false;
		}
	}

	inline stop_token::operator bool( ) const {
		return can_continue( );
	}

	inline void stop_token::wait( ) const {
		if( auto p = m_owner.lock( ); p ) {
			p->wait( );
		}
	}

	inline void stop_token::stop( ) {
		if( auto p = m_owner.lock( ); p ) {
			p->request_stop( );
		}
	}

	class ithread {
		std::shared_ptr<stop_token_owner> m_continue =
		  std::make_shared<stop_token_owner>( );
		::std::thread m_thread;

	public:
		using id = ::std::thread::id;

		template<typename Callable, typename... Args,
		         std::enable_if_t<
		           not std::is_same_v<daw::remove_cvref_t<Callable>, ithread>,
		           std::nullptr_t> = nullptr>
		explicit ithread( Callable &&callable, Args &&...args )
		  : m_thread(
		      []( auto func, stop_token ic, auto... lambda_args ) {
			      auto const on_exit = daw::on_scope_exit( [&] { ic.stop( ); } );
			      if constexpr( std::is_invocable_v<Callable, stop_token, Args...> ) {
				      return func( ic, lambda_args... );
			      } else {
				      return func( lambda_args... );
			      }
		      },
		      DAW_FWD( callable ), m_continue->get_interrupt_token( ),
		      DAW_FWD( args )... ) {}

		ithread( ithread && ) = delete;
		ithread( ithread const & ) = delete;
		ithread &operator=( ithread && ) = delete;
		ithread &operator=( ithread const & ) = delete;

		inline ~ithread( ) {
			try {
				m_continue->request_stop( );
				if( m_thread.joinable( ) ) {
					m_thread.join( );
				}
			} catch( ... ) {
				// Do not let an exception take us down
			}
		}

		[[nodiscard]] inline bool joinable( ) const {
			return m_continue->can_continue( ) and m_thread.joinable( );
		}

		[[nodiscard]] inline std::thread::id get_id( ) const noexcept {
			return m_thread.get_id( );
		}

		[[nodiscard]] inline static unsigned hardware_concurrency( ) noexcept {
			return std::thread::hardware_concurrency( );
		}

		inline void stop( ) {
			m_continue->request_stop( );
		}

		inline void join( ) {
			m_continue->wait( );
			m_thread.join( );
		}

		inline void stop_and_wait( ) {
			stop( );
			join( );
		}

		inline void detach( ) {
			m_thread.detach( );
		}
	};
} // namespace daw::parallel
