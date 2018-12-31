// The MIT License (MIT)
//
// Copyright (c) 2016-2018 Darrell Wright
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

#include <boost/next_prior.hpp>
#include "daw/fs/future_result.h"

namespace daw {
	namespace impl {
		member_data_t<void>::~member_data_t( ) = default;

		future_result_base_t::~future_result_base_t( ) noexcept = default;

		member_data_base_t::~member_data_base_t( ) noexcept = default;

		void member_data_t<void>::set_value(
		  member_data_t<void>::expected_result_t result ) {
			m_result = std::move( result );
			if( m_next ) {
				pass_next( std::move( m_result ) );
				return;
			}
			status( ) = future_status::ready;
			notify( );
		}

		void member_data_t<void>::set_value( ) {
			expected_result_t result;
			result = true;
			set_value( std::move( result ) );
		}

		void member_data_t<void>::set_exception( ) {
			set_exception( std::current_exception( ) );
		}

		void member_data_t<void>::set_exception( std::exception_ptr ptr ) {
			set_value( expected_result_t{ptr} );
		}

		member_data_base_t::member_data_base_t( task_scheduler ts )
		  : m_semaphore( )
		  , m_status( future_status::deferred )
		  , m_task_scheduler( std::move( ts ) ) {}

		member_data_base_t::member_data_base_t( daw::shared_latch sem,
		                                        task_scheduler ts )
		  : m_semaphore( std::move( sem ) )
		  , m_status( future_status::deferred )
		  , m_task_scheduler( std::move( ts ) ) {}

		void member_data_base_t::wait( ) const {
			if( m_status != future_status::ready ) {
				m_semaphore.wait( );
			}
		}

		bool member_data_base_t::try_wait( ) {
			return m_semaphore.try_wait( );
		}

		void member_data_base_t::notify( ) {
			m_semaphore.notify( );
		}

		future_status &member_data_base_t::status( ) {
			return m_status;
		}

		future_status const &member_data_base_t::status( ) const {
			return m_status;
		}
	} // namespace impl

	future_result_t<void>::future_result_t( )
	  : m_data( std::make_shared<m_data_t>( get_task_scheduler( ) ) ) {

		daw::exception::dbg_throw_on_false( m_data, "m_data shouldn't be null" );
	}

	future_result_t<void>::future_result_t( task_scheduler ts )
	  : m_data( std::make_shared<m_data_t>( std::move( ts ) ) ) {

		daw::exception::dbg_throw_on_false( m_data, "m_data shouldn't be null" );
	}

	future_result_t<void>::future_result_t( daw::shared_latch sem,
	                                        task_scheduler ts )
	  : m_data(
	      std::make_shared<m_data_t>( std::move( sem ), std::move( ts ) ) ) {

		daw::exception::dbg_throw_on_false( m_data, "m_data shouldn't be null" );
	}

	std::weak_ptr<future_result_t<void>::m_data_t>
	future_result_t<void>::weak_ptr( ) {
		return m_data;
	}

	void future_result_t<void>::wait( ) const {
		m_data->wait( );
	}

	void future_result_t<void>::get( ) const {
		m_data->wait( );
		m_data->m_result.get( );
	}

	bool future_result_t<void>::try_wait( ) const {
		return m_data->try_wait( );
	}

	future_result_t<void>::operator bool( ) const {
		return m_data->try_wait( );
	}

	void future_result_t<void>::set_value( ) {
		m_data->set_value( );
	}

	void future_result_t<void>::set_exception( ) {
		m_data->set_exception( std::current_exception( ) );
	}

	void future_result_t<void>::set_exception( std::exception_ptr ptr ) {
		m_data->set_exception( ptr );
	}

	bool future_result_t<void>::is_exception( ) const {
		wait( );
		return m_data->m_result.has_exception( );
	}
} // namespace daw
