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

#include <daw/daw_enable_if.h>
#include <daw/daw_move.h>
#include <daw/daw_traits.h>
#include <daw/parallel/daw_latch.h>

#include <functional>
#include <type_traits>

namespace daw {
	class [[nodiscard]] task_t {
		std::function<void( )> m_function{ }; // from ctor
		daw::shared_latch m_latch{ };

	public:
		explicit task_t( ) = default;

		template<typename Func,
		         ::std::enable_if_t<
		           not std::is_same_v<task_t, ::daw::remove_cvref_t<Func>>,
		           ::std::nullptr_t> = nullptr>
		explicit task_t( Func &&func )
		  : m_function( DAW_FWD( func ) ) {

			daw::exception::precondition_check( m_function,
			                                    "Callable must be valid" );
		}

		template<typename Func, typename Latch>
		explicit task_t( Func &&func, Latch l )
		  : m_function( DAW_FWD( func ) )
		  , m_latch( daw::is_shared_latch_v<Latch>
		               ? daw::move( l )
		               : daw::shared_latch( ::daw::move( l ) ) ) {

			static_assert( daw::is_shared_latch_v<Latch> or
			               daw::is_unique_latch_v<Latch> );

			daw::exception::precondition_check( m_function,
			                                    "Callable must be valid" );
		}

		inline void operator( )( ) {
			daw::exception::dbg_precondition_check( m_function,
			                                        "Callable must be valid" );
			(void)m_function( );
		}

		inline void operator( )( ) const {
			daw::exception::dbg_precondition_check( m_function,
			                                        "Callable must be valid" );
			(void)m_function( );
		}

		[[nodiscard]] inline bool is_ready( ) const {
			if( m_latch ) {
				return m_latch.try_wait( );
			}
			return true;
		}

		[[nodiscard]] inline explicit operator bool( ) const {
			return static_cast<bool>( m_function );
		}
	}; // namespace daw
} // namespace daw
