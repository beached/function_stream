// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/parallel
//

#pragma once

#include <daw/cpp_17.h>
#include <daw/daw_concepts.h>
#include <daw/daw_move.h>
#include <daw/parallel/daw_unique_mutex.h>

#include <ciso646>
#include <mutex>
#include <optional>
#include <utility>

namespace daw {
	template<typename T>
	class lockable_ptr_t {
		class locked_ptr_t {
			std::unique_lock<std::mutex> m_lock;
			daw::not_null<std::unique_ptr<T> *> m_ptr;

			locked_ptr_t( std::unique_lock<std::mutex> &&lck, std::unique_ptr<T> &value )
			  : m_lock( DAW_MOVE( lck ) )
			  , m_ptr( &value ) {}

			friend lockable_ptr_t;

		public:
			using ptr_type = std::unique_ptr<T>;
			using reference = T &;
			using const_reference = T const &;
			using pointer = T *;
			using const_pointer = T const *;

			locked_ptr_t( locked_ptr_t const &other ) = delete;
			locked_ptr_t &operator=( locked_ptr_t const &other ) = delete;

			locked_ptr_t( locked_ptr_t &&other ) noexcept = default;
			locked_ptr_t &operator=( locked_ptr_t && ) noexcept = default;

			void release( ) noexcept {
				m_lock.unlock( );
			}

			ptr_type &get( ) &noexcept {
				return *m_ptr;
			}

			ptr_type const &get( ) const &noexcept {
				return *m_ptr;
			}

			reference operator*( ) &noexcept {
				return *get( );
			}

			const_reference operator*( ) const &noexcept {
				return *get( );
			}

			pointer operator->( ) &noexcept {
				return get( ).get( );
			}

			const_pointer operator->( ) const &noexcept {
				return get( ).get( );
			}

			explicit operator bool( ) const {
				return static_cast<bool>( get( ) );
			}
		}; // locked_ptr_t

		class const_locked_ptr_t {
			std::unique_lock<std::mutex> m_lock;
			daw::not_null<std::unique_ptr<T> const *> m_ptr;

			const_locked_ptr_t( std::unique_lock<std::mutex> &&lck, std::unique_ptr<T> const &value )
			  : m_lock( DAW_MOVE( lck ) )
			  , m_ptr( &value ) {}

			friend lockable_ptr_t;

		public:
			using ptr_type = std::unique_ptr<T> const;
			using reference = T const &;
			using const_reference = T const &;
			using pointer = T const *;
			using const_pointer = T const *;

			const_locked_ptr_t( const_locked_ptr_t const &other ) = delete;
			const_locked_ptr_t &operator=( const_locked_ptr_t const &other ) = delete;

			const_locked_ptr_t( const_locked_ptr_t &&other ) noexcept = default;
			const_locked_ptr_t &operator=( const_locked_ptr_t && ) noexcept = default;

			void release( ) noexcept {
				m_lock.unlock( );
			}

			ptr_type &get( ) const &noexcept {
				return *m_ptr;
			}

			const_reference operator*( ) const &noexcept {
				return *get( );
			}

			const_pointer operator->( ) const &noexcept {
				return get( ).get( );
			}

			explicit operator bool( ) const {
				return static_cast<bool>( get( ) );
			}
		}; // locked_ptr_t

		mutable daw::basic_unique_mutex<std::mutex> m_mutex = daw::basic_unique_mutex<std::mutex>( );
		std::unique_ptr<T> m_ptr = nullptr;

	public:
		lockable_ptr_t( ) = default;

		explicit lockable_ptr_t( T *ptr ) noexcept
		  : m_ptr( ptr ) {}

		lockable_ptr_t( lockable_ptr_t &&other )
		  : m_ptr( other.release( ) ) {}

		lockable_ptr_t &operator=( lockable_ptr_t &&rhs ) {
			if( this != &rhs ) {
				auto const lck = std::unique_lock<std::mutex>( m_mutex.get( ) );
				m_ptr.reset( rhs.release( ) );
			}
			return *this;
		}

		locked_ptr_t get( ) {
			return locked_ptr_t( std::unique_lock<std::mutex>( m_mutex.get( ) ), m_ptr );
		}

		const_locked_ptr_t get( ) const {
			return const_locked_ptr_t( std::unique_lock<std::mutex>( m_mutex.get( ) ), m_ptr );
		}

		std::optional<locked_ptr_t> try_get( ) {
			auto lck = std::unique_lock<std::mutex>( m_mutex.get( ), std::try_to_lock );
			if( not lck.owns_lock( ) ) {
				return { };
			}
			return { locked_ptr_t( DAW_MOVE( lck ), *m_ptr ) };
		}

		std::optional<const_locked_ptr_t> try_get( ) const {
			auto lck = std::unique_lock<std::mutex>( m_mutex.get( ), std::try_to_lock );
			if( not lck.owns_lock( ) ) {
				return { };
			}
			return { const_locked_ptr_t( DAW_MOVE( lck ), *m_ptr ) };
		}

		void reset( ) {
			auto const lck = std::unique_lock<std::mutex>( m_mutex.get( ) );
			m_ptr.reset( );
		}

		T *release( ) {
			auto const lck = std::unique_lock<std::mutex>( m_mutex.get( ) );
			return m_ptr.release( );
		}

		explicit operator bool( ) const {
			auto const lck = std::unique_lock<std::mutex>( m_mutex.get( ) );
			return static_cast<bool>( m_ptr );
		}
	}; // lockable_ptr_t
} // namespace daw
