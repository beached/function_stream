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

#include <boost/any.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <daw/daw_expected.h>
#include "task_scheduler.h"

namespace daw {
	template<typename Function, typename... Functions>
	struct function_stream {
		using sfuple_t = std::shared_ptr<std::tuple<Function, Functions...>>;
		sfuple_t m_funcs;

		function_stream( Function func, Functions... funcs ):
				m_funcs{ std::make_shared<std::tuple<Function, Functions...>>( std::make_tuple( std::move( func ), std::move( funcs )... ) ) } { }

		template<typename Callback, typename... Args>
		void operator( )( Callback && cb, Args&&... args ) const {
			this->call<0>( m_funcs, std::forward<Callback>( cb ), std::forward<Args>( args )... );
		}

	private:
		template<size_t pos, typename Callback, typename... Args>
		void call( sfuple_t f, Callback cb, Args... args ) const {
			auto task_scheduler = get_task_scheduler( );
			task_scheduler.add_task( [=, funcs=std::move( f ), callback=std::move( cb )]( ) mutable {
				auto const & t = *funcs;
				auto func = std::get<pos>( t );
				auto result = func( std::move( args )... );
				if( pos == sizeof...( Functions ) ) {
					callback( std::move( result ) );
				} else {
					this->template call<pos+1>( std::move( funcs ), std::move( callback ), std::move( result ) );
				}
			} );
		}
	};	// function_stream

	template<typename Function, typename... Functions>
	auto make_function_stream( Function && func, Functions&&... funcs ) {
		return function_stream<Function, Functions...>{ std::forward<Function>( func ), std::forward<Functions>( funcs )... };
	}
}    // namespace daw

