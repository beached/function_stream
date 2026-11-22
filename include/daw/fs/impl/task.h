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
#include <daw/daw_scope_guard.h>
#include <daw/daw_traits.h>
#include <daw/parallel/daw_locked_value.h>

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

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
		  : m_latch( std::move( l ) )
		  , m_function( DAW_FWD( func ) ) {

			assert( m_latch );
			if( m_function ) {
				m_latch.add_notifier( );
			}
		}

		explicit fixed_task_t( invocable auto &&func, daw::unique_cnt_sem l )
		  : m_latch( std::move( l ) )
		  , m_function( DAW_FWD( func ) ) {

			if( not m_function ) {
				m_function = [] {};
			}
			assert( m_latch );
			m_latch.add_notifier( );
		}

		inline void execute( ) {
			auto const ae = on_scope_exit( [latch=m_latch]( ) mutable { latch.notify( ); } );
			m_function( );
		}

		[[nodiscard]] inline bool try_wait( ) const {
			return m_latch.try_wait( );
		}

		void wait( ) const {
			m_latch.wait( );
		}
	};

	/// A move only, type erased task.  The callable is stored inline when it is small
	/// enough and nothrow movable, so submitting a task does not allocate.  Otherwise it is
	/// allocated on the heap.  A task scheduled with a semaphore adds a notifier to it and
	/// notifies it once the callable has run.
	///
	/// Inline storage also avoids freeing a task on a different thread from the one that
	/// allocated it (tasks are created by the submitter and destroyed by a worker), which
	/// made glibc's malloc take its arena lock and contend with the submitter
	class [[nodiscard]] unique_task_t {
	public:
		/// Callables up to this size are stored inline.  Large enough for the scheduler's
		/// task_wrapper (an id and a weak_ptr) around a closure of a few pointers
		static constexpr std::size_t inline_size = 64;
		static constexpr std::size_t inline_align = alignof( std::max_align_t );

	private:
		struct ops_t {
			void ( *invoke )( void *storage );
			/// Move constructs into dst from src, then destroys src
			void ( *relocate )( void *dst, void *src ) noexcept;
			void ( *destroy )( void *storage ) noexcept;
		};

		template<typename F>
		static constexpr bool is_stored_inline_v =
		  sizeof( F ) <= inline_size and alignof( F ) <= inline_align and
		  std::is_nothrow_move_constructible_v<F>;

		template<typename F>
		static constexpr ops_t inline_ops = {
		  []( void *storage ) {
			  ( *static_cast<F *>( storage ) )( );
		  },
		  []( void *dst, void *src ) noexcept {
			  auto *const from = static_cast<F *>( src );
			  ::new( dst ) F( std::move( *from ) );
			  from->~F( );
		  },
		  []( void *storage ) noexcept {
			  static_cast<F *>( storage )->~F( );
		  } };

		// The storage holds an owning F *
		template<typename F>
		static constexpr ops_t heap_ops = {
		  []( void *storage ) {
			  ( **static_cast<F **>( storage ) )( );
		  },
		  []( void *dst, void *src ) noexcept {
			  ::new( dst ) F *( *static_cast<F **>( src ) );
		  },
		  []( void *storage ) noexcept {
			  delete *static_cast<F **>( storage );
		  } };

		alignas( inline_align ) mutable std::byte m_storage[inline_size];
		// Null when empty (default constructed or moved from)
		ops_t const *m_ops = nullptr;
		// Only set for tasks scheduled with a semaphore
		std::optional<shared_cnt_sem> m_latch{ };

		template<typename Task>
		void emplace( Task &&func ) {
			using F = std::remove_cvref_t<Task>;
			if constexpr( is_stored_inline_v<F> ) {
				::new( static_cast<void *>( m_storage ) ) F( DAW_FWD( func ) );
				m_ops = &inline_ops<F>;
			} else {
				::new( static_cast<void *>( m_storage ) ) F *( new F( DAW_FWD( func ) ) );
				m_ops = &heap_ops<F>;
			}
		}

		void reset( ) noexcept {
			if( m_ops ) {
				m_ops->destroy( m_storage );
				m_ops = nullptr;
			}
			m_latch.reset( );
		}

		void take( unique_task_t &other ) noexcept {
			if( other.m_ops ) {
				other.m_ops->relocate( m_storage, other.m_storage );
				m_ops = std::exchange( other.m_ops, nullptr );
			}
			m_latch = std::move( other.m_latch );
			other.m_latch.reset( );
		}

	public:
		unique_task_t( ) noexcept = default;
		unique_task_t( unique_task_t const & ) = delete;
		unique_task_t &operator=( unique_task_t const & ) = delete;

		unique_task_t( unique_task_t &&other ) noexcept {
			take( other );
		}

		unique_task_t &operator=( unique_task_t &&rhs ) noexcept {
			if( this != &rhs ) {
				reset( );
				take( rhs );
			}
			return *this;
		}

		~unique_task_t( ) {
			reset( );
		}

		/// False for an empty task (default constructed or moved from)
		[[nodiscard]] explicit operator bool( ) const noexcept {
			return m_ops != nullptr;
		}

		template<not_cvref_of<unique_task_t> Task>
		requires( invocable<Task> ) //
		  explicit unique_task_t( Task &&func ) {
			emplace( DAW_FWD( func ) );
		}

		explicit unique_task_t( invocable auto &&func, shared_cnt_sem l ) {
			emplace( DAW_FWD( func ) );
			// Only once the task exists, so a throwing allocation does not leave the
			// semaphore waiting on a notifier that never comes
			l.add_notifier( );
			m_latch.emplace( std::move( l ) );
		}

		explicit unique_task_t( invocable auto &&func, unique_cnt_sem l )
		  : unique_task_t( DAW_FWD( func ), shared_cnt_sem( std::move( l ) ) ) {}

		/// Runs the callable, then notifies the semaphore (if any), even if it threw
		void execute( ) const {
			assert( m_ops );
			auto const notify = on_scope_exit( [this]( ) noexcept {
				if( m_latch ) {
					auto latch = *m_latch;
					latch.notify( );
				}
			} );
			m_ops->invoke( m_storage );
		}

		/// True if the task's semaphore has been released.  A task without a semaphore
		/// reports false
		[[nodiscard]] bool try_wait( ) const {
			assert( m_ops );
			return m_latch and m_latch->try_wait( );
		}

		/// Waits for the task's semaphore.  Returns immediately for a task without one
		void wait( ) const {
			assert( m_ops );
			if( m_latch ) {
				m_latch->wait( );
			}
		}
	}; // namespace daw
} // namespace daw
