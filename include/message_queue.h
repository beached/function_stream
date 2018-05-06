// The MIT License (MIT)
//
// Copyright (c) 2017-2018 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
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
		constexpr unsigned long size_msg_queue_to_cache_size( ) {
			auto const sz = 4096;
			return sz / sizeof( T );
		}

		struct use_autosize {};

		template<typename T, typename base_queue_t>
		class basic_msg_queue_t {
			struct members_t {
				std::atomic<bool> m_completed;
				base_queue_t m_queue;

				members_t( )
				  : m_completed{false}
				  , m_queue{} {}
				members_t( unsigned long max_size )
				  : m_completed{false}
				  , m_queue{max_size} {}
				members_t( use_autosize )
				  : members_t{size_msg_queue_to_cache_size<T>( )} {}
			};
			std::shared_ptr<members_t> members;

			base_queue_t const &queue( ) const {
				return members->m_queue;
			}

			base_queue_t &queue( ) {
				return members->m_queue;
			}

		public:
			basic_msg_queue_t( )
			  : members{std::make_shared<members_t>( )} {}
			basic_msg_queue_t( unsigned long max_size )
			  : members{std::make_shared<members_t>( max_size )} {}
			basic_msg_queue_t( use_autosize )
			  : members{std::make_shared<members_t>( use_autosize{} )} {}

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
		using spsc_msg_queue_t = basic_msg_queue_t<T, boost::lockfree::spsc_queue<T>>;

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

			template<typename... Args>
			msg_ptr_t( Args &&... args )
			  : m_ptr{new value_t{std::forward<Args>( args )...}} {}

			msg_ptr_t( msg_ptr_t const &other ) noexcept
			  : m_ptr{std::exchange( other.m_ptr, nullptr )} {}
			msg_ptr_t &operator=( msg_ptr_t const &rhs ) noexcept {
				if( this != &rhs ) {
					m_ptr = std::exchange( rhs.m_ptr, nullptr );
				}
				return *this;
			}

			msg_ptr_t( msg_ptr_t && ) noexcept = default;
			msg_ptr_t &operator=( msg_ptr_t && ) noexcept = default;

			~msg_ptr_t( ) {
				auto tmp = std::exchange( m_ptr, nullptr );
				if( tmp ) {
					delete tmp;
				}
			}

			reference operator*( ) noexcept {
				return *m_ptr;
			}

			const_reference operator*( ) const noexcept {
				return *m_ptr;
			}

			pointer operator->( ) noexcept {
				return m_ptr;
			}

			const_pointer operator->( ) const noexcept {
				return m_ptr;
			}

			explicit operator bool( ) const {
				return static_cast<bool>( m_ptr );
			}

			value_t move_out( ) noexcept {
				auto tmp = std::exchange( m_ptr, nullptr );
				return std::move( *tmp );
			}

		private:
			mutable pointer m_ptr;
		};
	} // namespace parallel
} // namespace daw
