// The MIT License (MIT)
//
// Copyright (c) 2017 Darrell Wright
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
		template<typename T, typename base_queue_t>
		class basic_msg_queue_t {
			struct members_t {
				std::atomic<bool> m_completed;
				base_queue_t m_queue;

				members_t( ) : m_completed{false}, m_queue{} {}

				members_t( unsigned long max_size ) : m_completed{false}, m_queue{max_size} {}
			};
			std::shared_ptr<members_t> members;

			auto const &queue( ) const {
				return members->m_queue;
			}

			auto &queue( ) {
				return members->m_queue;
			}

		  public:
			basic_msg_queue_t( ) : members{std::make_shared<members_t>( )} {}

			basic_msg_queue_t( unsigned long max_size ) : members{std::make_shared<members_t>( max_size )} {}

			void notify_completed( ) {
				members->m_completed.store( true );
			}

			template<typename U>
			bool send( U const & value ) {
				return queue( ).push( value );
			}

			template<typename U>
			bool receive( U & value ) {
				return queue( ).pop( value );
			}

			bool has_more( ) const {
				return !members->m_completed.load( ) || !members->m_queue.empty( );
			}
		};	// basic_msg_queue_t

		template<typename T>
		using spsc_msg_queue_t = basic_msg_queue_t<T, boost::lockfree::spsc_queue<T>>;

		template<typename T>
		using mpmc_msg_queue_t = basic_msg_queue_t<T, boost::lockfree::stack<T>>;
	} // namespace parallel
} // namespace daw

