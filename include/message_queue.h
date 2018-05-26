// The MIT License (MIT)
//
// Copyright (c) 2017-2018 Darrell Wright
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

#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/stack.hpp>

namespace daw {
	namespace parallel {
		template<typename T>
		constexpr unsigned long size_msg_queue_to_cache_size( ) noexcept {
			auto const sz = 4096;
			return sz / sizeof( T );
		}

		struct use_autosize {};

		template<typename T, typename base_queue_t>
		class basic_msg_queue_t {
			struct members_t {
				std::atomic<bool> m_completed{ };
				base_queue_t m_queue;

				members_t( )
				  : m_completed( false )
				  , m_queue( ) {}

				explicit members_t( unsigned long max_size )
				  : m_completed( false )
				  , m_queue( max_size ) {}

				explicit members_t( use_autosize )
				  : m_completed( false )
				  , m_queue( size_msg_queue_to_cache_size<T>( ) ) {}
			};
			std::shared_ptr<members_t> members;

			base_queue_t const &queue( ) const {
				return members->m_queue;
			}

			base_queue_t &queue( ) {
				return members->m_queue;
			}

		public:
			basic_msg_queue_t( ) = default;

			explicit basic_msg_queue_t( unsigned long max_size )
			  : members( std::make_shared<members_t>( max_size ) ) {}

			explicit basic_msg_queue_t( use_autosize )
			  : members( std::make_shared<members_t>( use_autosize{} ) ) {}

			void notify_completed( ) {
				members->m_completed.store( true );
			}

			template<typename U>
			bool send( U const &value ) {
				return queue( ).push( value );
			}

			template<typename U>
			bool receive( U &value ) {
				return queue( ).pop( value );
			}

			bool has_more( ) const {
				return !members->m_completed.load( ) || !members->m_queue.empty( );
			}
		}; // basic_msg_queue_t

		template<typename T>
		using spsc_msg_queue_t =
		  basic_msg_queue_t<T, boost::lockfree::spsc_queue<T>>;

		template<typename T>
		using mpmc_msg_queue_t = basic_msg_queue_t<T, boost::lockfree::stack<T>>;

		/// msg_ptr_t only ever has one owner and will, like auto_ptr,
		/// do a move when copied.  It is intended to allow anything through
		/// the message queue and then moved_out to a real value afterwards.
		/// It will clean-up when it goes out of scope if it still owns a value
		template<typename T>
		struct msg_ptr_t {
			using value_t = std::remove_reference_t<T>;
			using pointer = value_t *;
			using const_pointer = value_t const *;
			using reference = value_t &;
			using const_reference = value_t const &;

		private:
			mutable pointer m_ptr{ };

		public:
			template<typename... Args,
			         std::enable_if_t<(!daw::traits::is_type_v<msg_ptr_t, Args...> &&
			                           !daw::traits::is_type_v<pointer, Args...>),
			                          std::nullptr_t> = nullptr>
			explicit msg_ptr_t( Args &&... args )
			  : m_ptr( new value_t( std::forward<Args>( args )... ) ) {}

			constexpr explicit msg_ptr_t( pointer p ) noexcept
			  : m_ptr( p ) {}

			constexpr msg_ptr_t( msg_ptr_t const &other ) noexcept
			  : m_ptr( std::exchange( other.m_ptr, nullptr ) ) {}

			constexpr msg_ptr_t &operator=( msg_ptr_t const &rhs ) noexcept {
				if( this != &rhs ) {
					m_ptr = std::exchange( rhs.m_ptr, nullptr );
				}
				return *this;
			}

			constexpr msg_ptr_t( msg_ptr_t && ) noexcept = default;
			constexpr msg_ptr_t &operator=( msg_ptr_t && ) noexcept = default;

			~msg_ptr_t( ) noexcept {
				auto tmp = std::exchange( m_ptr, nullptr );
				if( tmp ) {
					delete tmp;
				}
			}

			constexpr reference operator*( ) noexcept {
				return *m_ptr;
			}

			constexpr const_reference operator*( ) const noexcept {
				return *m_ptr;
			}

			constexpr pointer operator->( ) noexcept {
				return m_ptr;
			}

			constexpr const_pointer operator->( ) const noexcept {
				return m_ptr;
			}

			constexpr explicit operator bool( ) const {
				return static_cast<bool>( m_ptr );
			}

			constexpr value_t &&move_out( ) noexcept {
				auto tmp = std::exchange( m_ptr, nullptr );
				return std::move( *tmp );
			}
		};
	} // namespace parallel
} // namespace daw
