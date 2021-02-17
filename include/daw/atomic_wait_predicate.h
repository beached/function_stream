// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/
//

#pragma once

#include <daw/daw_assume.h>
#include <daw/daw_likely.h>

#include <atomic_wait>

namespace daw::parallel {
	template<typename T, typename Predicate,
	         std::enable_if_t<std::is_invocable_r_v<bool, Predicate, T>,
	                          std::nullptr_t> = nullptr>
	void atomic_wait_pred( std::atomic<T> const *ptr, Predicate &&pred,
	                       std::memory_order order = std::memory_order_seq_cst ) {
		assert( ptr );
		auto current = ptr->load( order );
		while( current != 0 and not pred( current ) ) {
			std::atomic_wait_explicit( ptr, current, std::memory_order_relaxed );
			current = ptr->load( order );
		}
	}
} // namespace daw::parallel
