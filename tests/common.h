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

#include "display_info.h"

namespace {
	template<typename Container>
	[[maybe_unused]] void show_times( Container const &times ) {
		auto first = std::begin( times );
		auto const last = std::end( times );
		assert( first != last );
		std::cout << '[' << daw::utility::format_seconds( *first, 2 );
		auto avg = *first;
		size_t count = 1;
		++first;
		while( first != last ) {
			std::cout << ", " << daw::utility::format_seconds( *first, 2 );
			avg += *first;
			++count;
			++first;
		}
		std::cout << "]\n";
		avg /= static_cast<double>( count );
		first = std::begin( times );
		double std_dev = abs( *first - avg );
		++first;
		while( first != last ) {
			std_dev += abs( *first - avg );
			++first;
		}
		std_dev /= static_cast<double>( count );
		std::cout << "avg= " << daw::utility::format_seconds( avg )
		          << " std_dev=" << daw::utility::format_seconds( std_dev ) << '\n';
	}

	// static constexpr size_t const MAX_ITEMS = 134'217'728;
	// static constexpr size_t const LARGE_TEST_SZ = 268'435'456;

#if not defined( DEBUG )
	static constexpr size_t const MAX_ITEMS = 4'194'304;
	// static constexpr size_t const MAX_ITEMS = 14'217'728;
	// static constexpr size_t const LARGE_TEST_SZ = 2 * MAX_ITEMS;
	static constexpr size_t const LARGE_TEST_SZ = 134'217'7280;
#else
	static constexpr size_t const MAX_ITEMS = 4'194'304;
	static constexpr size_t const LARGE_TEST_SZ = 2 * MAX_ITEMS;
#endif

	//	 static constexpr size_t const MAX_ITEMS = 1'217'728;
	//	 static constexpr size_t const LARGE_TEST_SZ = 2 * MAX_ITEMS;
} // namespace
