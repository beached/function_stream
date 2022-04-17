// The MIT License (MIT)
//
// Copyright (c) 2016-2019 Darrell Wright
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

#include <functional>
#include <type_traits>

#include <daw/daw_enable_if.h>
#include <daw/daw_traits.h>
#include <daw/parallel/daw_atomic_unique_ptr.h>
#include <daw/parallel/daw_latch.h>

namespace daw {
	class [[nodiscard]] task_t {
		struct impl_t {
			std::function<void( )> m_function; // from ctor
			// shared to interoperate with other parts
			daw::shared_latch m_latch = daw::shared_latch( );

			explicit impl_t( std::function<void( )> &&func )
			  : m_function( DAW_MOVE( func ) ) {}

			impl_t( std::function<void( )> &&func, daw::shared_latch &&l )
			  : m_function( DAW_MOVE( func ) )
			  , m_latch( DAW_MOVE( l ) ) {}
		};

		daw::atomic_unique_ptr<impl_t> m_impl = nullptr;

	public:
		task_t( ) = default;

		template<typename Func,
		         std::enable_if_t<
		           not std::is_same_v<task_t, daw::remove_cvref_t<Func>>,
		           std::nullptr_t> = nullptr>
		explicit task_t( Func && func )
		  : m_impl( daw::make_atomic_unique_ptr<impl_t>(
		      std::function<void( )>( DAW_FWD( func ) ) ) ) {

			assert( m_impl );
			daw::exception::precondition_check( m_impl->m_function,
			                                    "Callable must be valid" );
		}

		template<typename Func, typename Latch>
		task_t( Func && func, Latch l )
		  : m_impl( daw::make_atomic_unique_ptr<impl_t>(
		      std::function<void( )>( DAW_FWD( func ) ),
		      daw::is_shared_latch_v<Latch>
		        ? DAW_MOVE( l )
		        : daw::shared_latch( DAW_MOVE( l ) ) ) ) {

			assert( m_impl );
			static_assert( daw::is_shared_latch_v<Latch> or
			               daw::is_unique_latch_v<Latch> );

			daw::exception::precondition_check( m_impl->m_function,
			                                    "Callable must be valid" );
		}

		inline void operator( )( ) noexcept( noexcept( m_impl->m_function( ) ) ) {
			assert( m_impl );
			daw::exception::dbg_precondition_check( m_impl->m_function,
			                                        "Callable must be valid" );
			m_impl->m_function( );
		}

		inline void operator( )( )
		  const noexcept( noexcept( m_impl->m_function( ) ) ) {
			assert( m_impl );
			daw::exception::dbg_precondition_check( m_impl->m_function,
			                                        "Callable must be valid" );
			m_impl->m_function( );
		}

		[[nodiscard]] inline bool is_ready( ) const {
			assert( m_impl );
			if( m_impl->m_latch ) {
				return m_impl->m_latch.try_wait( );
			}
			return true;
		}

		[[nodiscard]] inline explicit operator bool( ) const noexcept {
			return static_cast<bool>( m_impl and m_impl->m_function );
		}
	}; // namespace daw
} // namespace daw
