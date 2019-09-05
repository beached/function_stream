// The MIT License (MIT)
//
// Copyright (c) 2019 Darrell Wright
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

#include <memory>
#include <type_traits>

#include <daw/daw_exception.h>

namespace daw {
	template<typename T, typename Container = ::std::unique_ptr<T>>
	class dbg_proxy {
		Container m_data = Container( );

	public:
		constexpr dbg_proxy( ) noexcept(
		  ::std::is_nothrow_default_constructible_v<Container> ) = default;

		template<
		  typename... Args,
		  ::std::enable_if_t<not daw::traits::is_first_type_v<dbg_proxy, Args...>,
		                     std::nullptr_t> = nullptr>
		constexpr dbg_proxy( Args &&... args ) noexcept(
		  ::std::is_nothrow_constructible_v<Container, Args...> )
		  : m_data( std::forward<Args>( args )... ) {}

		explicit constexpr operator bool( ) const
		  noexcept( noexcept( m_data.operator bool( ) ) ) {
			return static_cast<bool>( m_data );
		}

		constexpr decltype( auto )
		operator*( ) noexcept( noexcept( m_data.operator*( ) ) ) {
			::daw::exception::precondition_check( m_data );
			return m_data.operator*( );
		}

		constexpr decltype( auto ) operator*( ) const
		  noexcept( noexcept( m_data.operator*( ) ) ) {
			::daw::exception::precondition_check( m_data );
			return m_data.operator*( );
		}

		constexpr decltype( auto )
		operator-> ( ) noexcept( noexcept( m_data.operator->( ) ) ) {
			::daw::exception::precondition_check( m_data );
			return m_data.operator->( );
		}

		constexpr decltype( auto ) operator-> ( ) const
		  noexcept( noexcept( m_data.operator->( ) ) ) {
			::daw::exception::precondition_check( m_data );
			return m_data.operator->( );
		}

		constexpr decltype( auto ) get( ) noexcept( noexcept( m_data.get( ) ) ) {
			::daw::exception::precondition_check( m_data );
			return m_data.get( );
		}

		constexpr decltype( auto ) get( ) const
		  noexcept( noexcept( m_data.get( ) ) ) {
			::daw::exception::precondition_check( m_data );
			return m_data.get( );
		}

		constexpr decltype( auto )
		reset( ) noexcept( noexcept( m_data.reset( ) ) ) {
			return m_data.reset( );
		}
	};
} // namespace daw
