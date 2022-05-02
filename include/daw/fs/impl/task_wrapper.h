// The MIT License (MIT)
//
// Copyright (c) Darrell Wright
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

#include <daw/daw_concepts.h>

#include <cstddef>
#include <thread>

namespace daw::impl {
	template<typename Iterator, typename Handle>
	struct temp_task_runner;

	template<typename Handle, invocable Function>
	struct task_wrapper {
		std::size_t id;
		mutable Handle wself;
		mutable Function func;

		explicit task_wrapper( std::size_t Id, Handle const &hnd, Function const &f )
		  : id( Id )
		  , wself( hnd )
		  , func( f ) {}

		explicit task_wrapper( std::size_t Id, Handle const &hnd, Function &&f )
		  : id( Id )
		  , wself( hnd )
		  , func( DAW_MOVE( f ) ) {}

		void operator( )( ) const {
			auto self = wself.lock( );
			if( not self ) {
				return;
			}
			(void)func( );
			while( self->started( ) and self->run_next_task( id ) ) {
				std::this_thread::yield( );
			}
		}
	};

	template<typename Handle, invocable Function>
	task_wrapper( std::size_t, Handle, Function ) -> task_wrapper<Handle, Function>;
} // namespace daw::impl
