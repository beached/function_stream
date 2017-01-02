// The MIT License (MIT)
//
// Copyright (c) 2016 Darrell Wright
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

#include <atomic>
#include <boost/optional.hpp>
#include <functional>
#include <mutex>
#include <memory>
#include <thread>
#include <vector>

#include <daw/daw_semaphore.h>

namespace daw {
	namespace impl {
		template<typename T>
		class locked_stack_t {
			daw::semaphore m_semaphore;
			std::unique_ptr<std::mutex> m_mutex;
			std::vector<T> m_items;
		public:	
			locked_stack_t( ):
					m_semaphore{ },
					m_mutex{ std::make_unique<std::mutex>( ) },
					m_items{ } { }

			~locked_stack_t( ) = default;
			locked_stack_t( locked_stack_t && ) = default;
			locked_stack_t & operator=( locked_stack_t && ) = default;

			locked_stack_t( locked_stack_t const & ) = delete;
			locked_stack_t & operator=( locked_stack_t const & ) = delete;

			boost::optional<T> try_pop_back( ) {
				if( !m_semaphore.try_wait( ) ) { 
					return { };
				}
				std::lock_guard<std::mutex> lock( *m_mutex );
				if( m_items.empty( ) ) {
					return { };
				}
				auto result = m_items.back( );
				m_items.pop_back( );
				return result;
			}

			T pop_back( ) {
				m_semaphore.wait( );
				std::lock_guard<std::mutex> lock( *m_mutex );
				if( m_items.empty( ) ) {
					return { };
				}
				auto result = m_items.back( );
				m_items.pop_back( );
				return result;
			}

			template<typename U>
			void push_back( U && value ) {
				std::lock_guard<std::mutex> lock( *m_mutex );
				m_items.push_back( std::forward<U>( value ) );
				m_semaphore.notify( );
			}

			template<typename... Args>
			void emplace_back( Args&&... args ) {
				std::lock_guard<std::mutex> lock( *m_mutex );
				m_items.emplace_back( std::forward<Args>( args )... );
				m_semaphore.notify( );
			}

			bool empty( ) {
				if( m_semaphore.try_wait( ) ) {
					m_semaphore.notify( );
					return false;
				}
				return true;
			}

			size_t size( ) {
				if( m_semaphore.try_wait( ) ) {
					std::lock_guard<std::mutex> lock( *m_mutex );
					auto result = m_items.size( );
					m_semaphore.notify( );
					return result;
				}
				return 0;
			}
		};	// locked_stack_t
	}	// namespace impl 

	struct task_scheduler_impl: public std::enable_shared_from_this<task_scheduler_impl> {
		using task_t = std::function<void( )>;
	private:
		using task_queue_t = impl::locked_stack_t<task_t>;
		std::vector<std::thread> m_threads;
		std::vector<task_queue_t> m_tasks;
		std::atomic_bool m_continue;
		std::weak_ptr<task_scheduler_impl> get_weak_this( );
		bool m_block_on_destruction;
		size_t m_num_threads;
		std::atomic_uintmax_t m_task_count;
	public:
		task_scheduler_impl( std::size_t num_threads, bool block_on_destruction );
		~task_scheduler_impl( );
		task_scheduler_impl( task_scheduler_impl && ) = default;
		task_scheduler_impl & operator=( task_scheduler_impl && ) = default;

		task_scheduler_impl( task_scheduler_impl const & ) = delete;
		task_scheduler_impl & operator=( task_scheduler_impl const & ) = delete;

		void add_task( task_t task ) noexcept;
		void start( );
		void stop( bool block = true ) noexcept;
		bool started( ) const;
	};	// task_scheduler_impl

	class task_scheduler {
		std::shared_ptr<task_scheduler_impl> m_impl;
	public:
		task_scheduler( std::size_t num_threads = std::thread::hardware_concurrency( ), bool block_on_destruction = true );
		~task_scheduler( ) = default;
		task_scheduler( task_scheduler && ) = default;
		task_scheduler & operator=( task_scheduler && ) = default;

		task_scheduler( task_scheduler const & ) = default;
		task_scheduler & operator=( task_scheduler const & ) = default;

		void add_task( task_scheduler_impl::task_t task ) noexcept;
		void start( );
		void stop( bool block = true ) noexcept;
		bool started( ) const;
	};	// task_scheduler

	task_scheduler get_task_scheduler( );
}    // namespace daw

