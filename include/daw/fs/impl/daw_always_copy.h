// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/function_stream
//

#pragma once

namespace daw {
	template<typename T>
	struct always_copy : T {
		always_copy( ) = default;
		~always_copy( ) = default;
		always_copy( always_copy const & ) = default;
		always_copy & operator=( always_copy const & ) = default;
		/*
		always_copy( always_copy const & ); not declared
		always_copy & operator=( always_copy const & ); not declared
		*/
	};
}

