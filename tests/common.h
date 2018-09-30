// The MIT License (MIT)
//
// Copyright (c) 2018 Darrell Wright
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

#include "display_info.h"

namespace {
	// static constexpr size_t const MAX_ITEMS = 134'217'728;
	// static constexpr size_t const LARGE_TEST_SZ = 268'435'456;

	static constexpr size_t const MAX_ITEMS = 14'217'728;
	static constexpr size_t const LARGE_TEST_SZ = 28'435'456;

	// static constexpr size_t const MAX_ITEMS = 4'217'728;
	// static constexpr size_t const LARGE_TEST_SZ = 2 * MAX_ITEMS;

#ifndef NOBOOST	
	BOOST_AUTO_TEST_CASE( start_task_scheduler ) {
		// Prime task scheduler so we don't pay to start it up in first test
		auto ts = daw::get_task_scheduler( );
		BOOST_REQUIRE( ts.started( ) );
		daw::do_not_optimize( ts );
	}
#endif
} // namespace
