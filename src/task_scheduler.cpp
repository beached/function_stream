// The MIT License (MIT)
//
// Copyright (c) 2016-2019 Darrell Wright
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

#include "daw/fs/task_scheduler.h"

namespace daw {

	void task_scheduler::start( ) {
		m_impl.start( );
	}

	void task_scheduler::stop( bool block ) noexcept {
		m_impl.stop( block );
	}

	bool task_scheduler::started( ) const {
		return m_impl.started( );
	}

	size_t task_scheduler::size( ) const {
		return m_impl.size( );
	}

	task_scheduler get_task_scheduler( ) {
		static auto ts = []( ) {
			task_scheduler result;
			result.start( );
			return result;
		}( );
		if( !ts ) {
			ts.start( );
		}
		return ts;
	}
} // namespace daw
