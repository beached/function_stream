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

#include <memory>
#include <thread>
#include <type_traits>

#include <daw/parallel/daw_latch.h>

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

	namespace ithread_impl {
		struct ithread_impl {
			interrupt_token_owner m_continue;
			::std::thread m_thread;

			template<typename Callable, typename... Args,
			         ::std::enable_if_t<
			           ::std::is_invocable_v<Callable, interrupt_token, Args...>,
			           ::std::nullptr_t> = nullptr>
			explicit ithread_impl( Callable &&callable, Args &&... args )
			  : m_continue( )
			  , m_thread( std::forward<Callable>( callable ),
			              m_continue.get_interrupt_token( ),
			              std::forward<Args>( args )... ) {}

			template<typename Callable, typename... Args,
			         ::std::enable_if_t<
			           not std::is_invocable_v<Callable, interrupt_token, Args...>,
			           ::std::nullptr_t> = nullptr>
			explicit ithread_impl( Callable &&callable, Args &&... args )
			  : m_continue( )
			  , m_thread( std::forward<Callable>( callable ),
			              std::forward<Args>( args )... ) {}
		};
	} // namespace ithread_impl

	class ithread {

		::std::unique_ptr<ithread_impl::ithread_impl> m_impl =
		  ::std::unique_ptr<ithread_impl::ithread_impl>( );

	public:
		constexpr ithread( ) noexcept = default;
		using id = ::std::thread::id;
		template<
		  typename Callable, typename... Args,
		  ::std::enable_if_t<::std::is_constructible_v<ithread_impl::ithread_impl,
		                                               Callable, Args...>,
		                     ::std::nullptr_t> = nullptr>
		explicit ithread( Callable &&callable, Args &&... args )
		  : m_impl( ::std::make_unique<ithread_impl::ithread_impl>(
		      std::forward<Callable>( callable ),
		      std::forward<Args>( args )... ) ) {}

		ithread( ithread const & ) = delete;
		ithread &operator=( ithread const & ) = delete;
		ithread( ithread && ) noexcept = default;
		ithread &operator=( ithread && ) noexcept = default;

		[[nodiscard]] inline bool joinable( ) const noexcept {
			assert( m_impl );
			return m_impl->m_thread.joinable( );
		}

		[[nodiscard]] inline std::thread::id get_id( ) const noexcept {
			assert( m_impl );
			return m_impl->m_thread.get_id( );
		}

		inline decltype( auto ) native_handle( ) {
			assert( m_impl );
			return m_impl->m_thread.native_handle( );
		}

		inline static unsigned int hardware_concurrency( ) noexcept {
			return std::thread::hardware_concurrency( );
		}

		void stop( ) {
			assert( m_impl );
			m_impl->m_continue.stop( );
		}

		void stop_and_wait( ) {
			assert( m_impl );
			m_impl->m_continue.stop( );
			m_impl->m_thread.join( );
		}

		inline void join( ) {
			assert( m_impl );
			m_impl->m_thread.join( );
		}

		inline void detach( ) {
			assert( m_impl );
			m_impl->m_thread.detach( );
		}

		inline ~ithread( ) {
			if( auto tmp = ::std::move( m_impl ); tmp ) {
				tmp->m_continue.stop( );
				if( not tmp->m_thread.joinable( ) ) {
					return;
				}
				try {
					tmp->m_thread.join( );
				} catch( ... ) {
					// Prevent taking down the system
				}
			}
		}
	};
} // namespace daw::parallel
