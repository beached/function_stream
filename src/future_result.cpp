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
	future_result_base_t::~future_result_base_t( ) = default;

	future_result_base_t::operator bool( ) const {
		return this->try_wait( );
	}

	future_result_t<void>::member_data_t::~member_data_t( ) = default;
	future_result_t<void>::member_data_t::member_data_t( ) : m_status{future_status::deferred} {}

	future_result_t<void>::member_data_t::member_data_t( daw::shared_semaphore semaphore )
	    : m_semaphore{std::move( semaphore )}, m_status{future_status::deferred} {}

	void future_result_t<void>::member_data_t::set_value( ) noexcept {
		m_result = true;
		m_status = future_status::ready;
		m_semaphore.notify( );
	}

	void future_result_t<void>::member_data_t::set_value( member_data_t &other ) {
		m_result = std::move( other.m_result );
		m_status = other.m_status;
		m_semaphore.notify( );
	}

	void future_result_t<void>::member_data_t::from_exception( std::exception_ptr ptr ) {
		m_result = std::move( ptr );
		m_status = future_status::ready;
		m_semaphore.notify( );
	}

	future_result_t<void>::future_result_t( ) : m_data{std::make_shared<member_data_t>( )} {}
	future_result_t<void>::future_result_t( daw::shared_semaphore semaphore )
	    : m_data{std::make_shared<member_data_t>( std::move( semaphore ) )} {}

	future_result_t<void>::~future_result_t( ) = default;

	std::weak_ptr<future_result_t<void>::member_data_t> future_result_t<void>::weak_ptr( ) {
		return m_data;
	}

	void future_result_t<void>::wait( ) const {
		m_data->m_semaphore.wait( );
	}

	void future_result_t<void>::get( ) const {
		wait( );
		m_data->m_result.get( );
	}

	bool future_result_t<void>::try_wait( ) const {
		return m_data->m_semaphore.try_wait( );
	}

	future_result_t<void>::operator bool( ) const {
		return try_wait( );
	}

	void future_result_t<void>::set_value( ) noexcept {
		m_data->set_value( );
	}

	bool future_result_t<void>::is_exception( ) const {
		wait( );
		return m_data->m_result.has_exception( );
	}
} // namespace daw
