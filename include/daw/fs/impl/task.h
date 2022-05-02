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

#include "daw_latch.h"
#include "daw_locked_ptr.h"

#include <daw/daw_enable_if.h>
#include <daw/daw_traits.h>
#include <daw/parallel/daw_locked_value.h>

#include <atomic>
#include <functional>
#include <memory>
#include <type_traits>

namespace daw {
	class [[nodiscard]] fixed_task_t {
		daw::shared_cnt_sem m_latch{ 0 }; // shared to interoperate with other parts
		std::function<void( )> m_function = { };

	public:
		fixed_task_t( ) = default;
		fixed_task_t( fixed_task_t const & ) = delete;
		fixed_task_t( fixed_task_t && ) = delete;
		fixed_task_t &operator=( fixed_task_t const & ) = delete;
		fixed_task_t &operator=( fixed_task_t && ) = delete;
		~fixed_task_t( ) = default;

		template<not_cvref_of<fixed_task_t> Task>
		requires( invocable<Task> ) //
		  explicit fixed_task_t( Task &&func )
		  : m_latch( 1 )
		  , m_function( DAW_FWD( func ) ) {
			assert( m_function );
		}

		explicit fixed_task_t( invocable auto &&func, daw::shared_cnt_sem l )
		  : m_latch( DAW_MOVE( l ) )
		  , m_function( DAW_FWD( func ) ) {

			assert( m_latch );
			if( m_function ) {
				m_latch.add_notifier( );
			}
		}

		explicit fixed_task_t( invocable auto &&func, daw::unique_cnt_sem l )
		  : m_latch( DAW_MOVE( l ) )
		  , m_function( DAW_FWD( func ) ) {

			if( not m_function ) {
				m_function = [] {};
			}
			assert( m_latch );
			m_latch.add_notifier( );
		}

		inline void execute( ) {
			auto const ae = on_scope_exit( [m_latch = m_latch]( ) mutable { m_latch.notify( ); } );
			m_function( );
		}

		[[nodiscard]] inline bool try_wait( ) const {
			return m_latch.try_wait( );
		}

		void wait( ) const {
			m_latch.wait( );
		}
	};

	class [[nodiscard]] unique_task_t {
		std::unique_ptr<fixed_task_t> m_ftask = std::make_unique<fixed_task_t>( );

	public:
		unique_task_t( ) = default;
		unique_task_t( unique_task_t const & ) = delete;
		unique_task_t &operator=( unique_task_t const & ) = delete;
		unique_task_t( unique_task_t &&other ) noexcept
		  : m_ftask( DAW_MOVE( other.m_ftask ) ) {
			other.m_ftask = std::make_unique<fixed_task_t>( );
		}

		unique_task_t &operator=( unique_task_t &&rhs ) noexcept {
			if( this != &rhs ) {
				m_ftask.reset( rhs.m_ftask.release( ) );
				rhs.m_ftask.reset( new fixed_task_t( ) );
			}
			return *this;
		}

		template<not_cvref_of<unique_task_t> Task>
		requires( invocable<Task> ) //
		  explicit unique_task_t( Task &&func )
		  : m_ftask( std::make_unique<fixed_task_t>( DAW_FWD( func ) ) ) {}

		explicit unique_task_t( invocable auto &&func, unique_cnt_sem l )
		  : m_ftask( std::make_unique<fixed_task_t>( DAW_FWD( func ), DAW_MOVE( l ) ) ) {}

		explicit unique_task_t( invocable auto &&func, shared_cnt_sem l )
		  : m_ftask( std::make_unique<fixed_task_t>( DAW_FWD( func ), DAW_MOVE( l ) ) ) {}

		inline void execute( ) const {
			assert( m_ftask );
			m_ftask->execute( );
		}

		[[nodiscard]] inline bool try_wait( ) const {
			assert( m_ftask );
			return m_ftask->try_wait( );
		}

		void wait( ) const {
			assert( m_ftask );
			m_ftask->wait( );
		}
	}; // namespace daw
} // namespace daw
