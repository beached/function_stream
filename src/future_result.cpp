// The MIT License (MIT)
//
// Copyright (c) 2016-2017 Darrell Wright
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

#include "future_result.h"

namespace daw {
	namespace impl {
		member_data_t<void>::~member_data_t( ) = default;

		future_result_base_t::~future_result_base_t( ) = default;

		member_data_base_t::~member_data_base_t( ) = default;

		void member_data_t<void>::set_value( member_data_t<void>::expected_result_t result ) noexcept {
			m_result = result;
			//	std::move( result );
			if( m_next ) {
				pass_next( std::move( m_result ) );
				return;
			}
			status( ) = future_status::ready;
			notify( );
		}

		void member_data_t<void>::set_value( ) noexcept {
			expected_result_t result;
			result = true;
			set_value( std::move( result ) );
		}

		void member_data_t<void>::set_exception( ) noexcept {
			set_exception( std::current_exception( ) );
		}

		void member_data_t<void>::set_exception( std::exception_ptr ptr ) noexcept {
			set_value( expected_result_t{ptr} );
		}
	} // namespace impl

	future_result_t<void>::future_result_t( task_scheduler ts ) : m_data{std::make_shared<m_data_t>( std::move( ts ) )} {}

	future_result_t<void>::future_result_t( daw::shared_semaphore sem, task_scheduler ts )
	  : m_data{std::make_shared<m_data_t>( std::move( sem ), std::move( ts ) )} {}

	future_result_t<void>::~future_result_t( ) = default;

	std::weak_ptr<future_result_t<void>::m_data_t> future_result_t<void>::weak_ptr( ) {
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

	void future_result_t<void>::set_value( ) noexcept {
		m_data->set_value( );
	}

	void future_result_t<void>::set_exception( ) {
		m_data->set_exception( std::current_exception( ) );
	}

	bool future_result_t<void>::is_exception( ) const {
		wait( );
		return m_data->m_result.has_exception( );
	}
} // namespace daw
