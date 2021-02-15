// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/header_libraries
//

#pragma once

#include <daw/daw_move.h>

#include <ciso646>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <type_traits>

namespace daw {
	template<typename Mutex, typename ConditionVariable>
	class basic_condition_variable {
		Mutex m_mutex{ };
		ConditionVariable m_condition{ };

	public:
		basic_condition_variable( ) = default;

		void notify_all( ) {
			m_condition.notify_all( );
		}

		void notify_one( ) {
			m_condition.notify_one( );
		}

		template<typename Predicate>
		std::unique_lock<Mutex> wait( Predicate &&pred ) {
			auto lock = get_lock( );
			m_condition.wait( lock, pred );
			return daw::move( lock );
		}

		template<typename Rep, typename Period, typename Predicate>
		[[nodiscard]] std::pair<std::unique_lock<Mutex>, std::cv_status>
		wait_for( std::chrono::duration<Rep, Period> const &rel_time,
		          Predicate &&pred ) {
			auto const lock = get_lock( );
			std::condition_variable c;
			return { daw::move( lock ),
			         m_condition.wait_for( lock, rel_time, pred ) };
		}

		[[nodiscard]] std::unique_lock<Mutex> get_lock( ) {
			try {
				return std::unique_lock<Mutex>( m_mutex );
			} catch( std::system_error const &se ) {
				std::cerr << "Error getting lock: " << se.code( ) << ", " << se.what( )
				          << '\n';
				throw;
			}
		}
	}; // basic_condition_variable

	using condition_variable =
	  basic_condition_variable<std::mutex, std::condition_variable>;

	template<typename Mutex, typename ConditionVariable>
	class basic_unique_condition_variable {
		std::unique_ptr<basic_condition_variable<Mutex, ConditionVariable>>
		  m_members =
		    std::make_unique<basic_condition_variable<Mutex, ConditionVariable>>( );

	public:
		basic_unique_condition_variable( ) = default;

		basic_condition_variable<Mutex, ConditionVariable> *release( ) {
			return m_members.release( );
		}

		void notify_all( ) {
			assert( m_members );
			m_members->notify_all( );
		}

		void notify_one( ) {
			assert( m_members );
			m_members->notify_one( );
		}

		template<typename Predicate>
		std::unique_lock<Mutex> wait( Predicate &&pred ) {
			assert( m_members );
			return m_members->wait( DAW_FWD( pred ) );
		}

		template<typename Rep, typename Period, typename Predicate>
		[[nodiscard]] decltype( auto )
		wait_for( std::chrono::duration<Rep, Period> const &rel_time,
		          Predicate &&pred ) {

			assert( m_members );
			return m_members->wait_for( rel_time, DAW_FWD( pred ) );
		}

		template<typename Clock, typename Duration, typename Predicate>
		[[nodiscard]] decltype( auto )
		wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time,
		            Predicate &&pred ) {
			assert( m_members );
			return m_members->wait_for( timeout_time, DAW_FWD( pred ) );
		}

		[[nodiscard]] std::unique_lock<Mutex> get_lock( ) {
			assert( m_members );
			return m_members->get_lock( );
		}
	}; // basic_condition_variable

	using unique_condition_variable =
	  basic_unique_condition_variable<std::mutex, std::condition_variable>;

	template<typename Mutex, typename ConditionVariable>
	class basic_shared_condition_variable {
		std::shared_ptr<basic_condition_variable<Mutex, ConditionVariable>>
		  m_members =
		    std::make_shared<basic_condition_variable<Mutex, ConditionVariable>>( );

	public:
		basic_shared_condition_variable( ) = default;

		basic_shared_condition_variable(
		  basic_unique_condition_variable<Mutex, ConditionVariable> &&other )
		  : m_members( other.release( ) ) {}

		void notify_all( ) {
			assert( m_members );
			m_members->notify_all( );
		}

		void notify_one( ) {
			assert( m_members );
			m_members->notify_one( );
		}

		template<typename Predicate>
		std::unique_lock<Mutex> wait( Predicate &&pred ) {
			assert( m_members );
			return m_members->wait( DAW_FWD( pred ) );
		}

		template<typename Rep, typename Period, typename Predicate>
		[[nodiscard]] decltype( auto )
		wait_for( std::chrono::duration<Rep, Period> const &rel_time,
		          Predicate &&pred ) {

			assert( m_members );
			return m_members->wait_for( rel_time, DAW_FWD( pred ) );
		}

		template<typename Clock, typename Duration, typename Predicate>
		[[nodiscard]] decltype( auto )
		wait_until( std::chrono::time_point<Clock, Duration> const &timeout_time,
		            Predicate &&pred ) {
			assert( m_members );
			return m_members->wait_for( timeout_time, DAW_FWD( pred ) );
		}

		[[nodiscard]] std::unique_lock<Mutex> get_lock( ) {
			assert( m_members );
			return m_members->get_lock( );
		}
	}; // basic_condition_variable

	using shared_condition_variable =
	  basic_shared_condition_variable<std::mutex, std::condition_variable>;
} // namespace daw
