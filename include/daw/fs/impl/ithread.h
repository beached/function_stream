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

#include <daw/daw_concepts.h>
#include <daw/daw_utility.h>
#include <daw/parallel/daw_atomic_unique_ptr.h>
#include <daw/parallel/daw_latch.h>

#include <memory>
#include <thread>
#include <type_traits>

namespace daw::parallel {
	class interrupt_token_owner;

	class interrupt_token {
		daw::latch const *m_condition;

		friend class daw::parallel::interrupt_token_owner;

		explicit interrupt_token( daw::latch const &cond ) noexcept
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
		daw::latch m_condition = daw::latch( 1 );

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
			std::thread m_thread;
			daw::latch m_sem = daw::latch( 1 );

			template<typename Callable, typename... Args>
			  requires( invocable<Callable, interrupt_token, Args...> )
			explicit ithread_impl( Callable &&callable, Args &&...args )
			  : m_continue( )
			  , m_thread( DAW_FWD( callable ), m_continue.get_interrupt_token( ),
			              DAW_FWD( args )... ) {

				m_thread.detach( );
			}

			template<typename Callable, typename... Args>
			  requires( not invocable<Callable, interrupt_token, Args...> and
			            invocable<Callable, Args...> )
			explicit ithread_impl( Callable &&callable, Args &&...args )
			  : m_continue( )
			  , m_thread(
			      [&, callable = daw::mutable_capture( DAW_FWD( callable ) )](
			        auto &&...arguments ) {
				      auto const on_exit =
				        daw::on_scope_exit( [&]( ) { m_sem.notify( ); } );
				      return DAW_MOVE( *callable )(
				        std::forward<decltype( arguments )>( arguments )... );
			      },
			      DAW_FWD( args )... ) {}

			ithread_impl( ithread_impl && ) = delete;
			ithread_impl( ithread_impl const & ) = delete;
			ithread_impl &operator=( ithread_impl && ) = delete;
			ithread_impl &operator=( ithread_impl const & ) = delete;

			inline ~ithread_impl( ) noexcept {
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
		};
	} // namespace ithread_impl

	class ithread {

		std::unique_ptr<ithread_impl::ithread_impl> m_impl =
		  std::unique_ptr<ithread_impl::ithread_impl>( );

	public:
		constexpr ithread( ) noexcept = default;

		using id = std::thread::id;
		template<typename Callable, typename... Args,
		         std::enable_if_t<std::is_constructible_v<
		                            ithread_impl::ithread_impl, Callable, Args...>,
		                          std::nullptr_t> = nullptr>
		explicit ithread( Callable &&callable, Args &&...args )
		  : m_impl( std::make_unique<ithread_impl::ithread_impl>(
		      DAW_FWD( callable ), DAW_FWD( args )... ) ) {}

		[[nodiscard]] inline bool joinable( ) const noexcept {
			assert( m_impl );
			return m_impl->m_sem.try_wait( );
		}

		[[nodiscard]] inline std::thread::id get_id( ) const noexcept {
			assert( m_impl );
			return m_impl->m_thread.get_id( );
		}

		inline static unsigned int hardware_concurrency( ) noexcept {
			return std::thread::hardware_concurrency( );
		}

		void stop( ) {
			assert( m_impl );
			m_impl->m_continue.stop( );
		}

		inline void join( ) {
			assert( m_impl );
			m_impl->m_sem.wait( );
		}

		void stop_and_wait( ) {
			assert( m_impl );
			stop( );
			join( );
		}

		inline void detach( ) {
			assert( m_impl );
			m_impl->m_sem.reset( 0 );
		}
	};
} // namespace daw::parallel
