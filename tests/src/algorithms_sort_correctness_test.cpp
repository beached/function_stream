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

// Correctness tests for parallel::sort and parallel::stable_sort, as opposed to the
// benchmarks in the other sort tests:
//  - results match std::sort / std::stable_sort, including stability
//  - move only, non default constructible element types (the merge buffer holds
//    uninitialized storage)
//  - a comparator that throws: the sort must not return until every task has
//    finished, and must rethrow the exception

#include "daw/fs/algorithms.h"

#include <daw/daw_random.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
	int g_failures = 0;

	void check( bool condition, char const *what, std::size_t n ) {
		if( not condition ) {
			std::cerr << "FAILED: " << what << " (n = " << n << ")\n";
			++g_failures;
		}
	}

	// Includes sizes smaller than the number of threads and sizes that do not divide
	// evenly between them
	constexpr std::size_t test_sizes[] = { 0, 1, 7, 8, 100, 1023, 4096, 65537 };

	struct key_value {
		int key;
		int seq;
	};

	void stable_sort_is_stable( daw::task_scheduler ts ) {
		for( std::size_t const n : test_sizes ) {
			// Only 13 distinct keys, so there are many equal keys; seq records the
			// original order
			auto values = std::vector<key_value>( n );
			for( std::size_t i = 0; i < n; ++i ) {
				values[i] = { static_cast<int>( ( i * 7919 ) % 13 ), static_cast<int>( i ) };
			}
			auto const by_key = []( key_value const &lhs, key_value const &rhs ) {
				return lhs.key < rhs.key;
			};
			auto expected = values;
			std::stable_sort( expected.begin( ), expected.end( ), by_key );
			daw::algorithm::parallel::stable_sort(
			  values.data( ), values.data( ) + values.size( ), ts, by_key );
			check( std::equal( values.begin( ),
			                   values.end( ),
			                   expected.begin( ),
			                   []( key_value const &lhs, key_value const &rhs ) {
				                   return lhs.key == rhs.key and lhs.seq == rhs.seq;
			                   } ),
			       "stable_sort keeps equal keys in their original order",
			       n );
		}
	}

	void sort_matches_std_sort( daw::task_scheduler ts ) {
		for( std::size_t const n : test_sizes ) {
			auto values = daw::make_random_data<int64_t>( n );
			auto expected = values;
			std::sort( expected.begin( ), expected.end( ) );

			daw::algorithm::parallel::sort( values.data( ), values.data( ) + values.size( ), ts );
			check( values == expected, "sort of random data", n );

			daw::algorithm::parallel::sort( values.data( ), values.data( ) + values.size( ), ts );
			check( values == expected, "sort of already sorted data", n );

			std::reverse( values.begin( ), values.end( ) );
			daw::algorithm::parallel::sort( values.data( ), values.data( ) + values.size( ), ts );
			check( values == expected, "sort of reverse sorted data", n );
		}
	}

	void sort_move_only_type( daw::task_scheduler ts ) {
		auto const by_value = []( std::unique_ptr<int> const &lhs, std::unique_ptr<int> const &rhs ) {
			return *lhs < *rhs;
		};
		for( std::size_t const n : test_sizes ) {
			auto values = std::vector<std::unique_ptr<int>>( );
			values.reserve( n );
			for( std::size_t i = 0; i < n; ++i ) {
				values.push_back( std::make_unique<int>( static_cast<int>( ( i * 31337 ) % 1000 ) ) );
			}
			daw::algorithm::parallel::sort( values.data( ), values.data( ) + values.size( ), ts, by_value );
			check( std::all_of( values.begin( ),
			                    values.end( ),
			                    []( auto const &p ) {
				                    return static_cast<bool>( p );
			                    } ),
			       "sort of unique_ptr loses no elements",
			       n );
			check( std::is_sorted( values.begin( ), values.end( ), by_value ), "sort of unique_ptr", n );
		}
	}

	// Before this was fixed, a failed chunk completed the final future straight away, so
	// sort returned while other tasks were still sorting and merging the caller's data
	// (a use after free once the caller freed it), and the exception was lost
	void throwing_comparator( daw::task_scheduler ts ) {
		constexpr std::size_t n = 8192;
		// Throw during the chunk sorts (small limits) and during the merges (large limits)
		constexpr long throw_after[] = { 10, 1'000, 20'000, 60'000, 90'000, 120'000 };
		for( int round = 0; round < 5; ++round ) {
			for( long const limit : throw_after ) {
				auto compares = std::make_shared<std::atomic<long>>( 0 );
				auto const cmp = [compares, limit]( auto const &lhs, auto const &rhs ) {
					if( compares->fetch_add( 1, std::memory_order_relaxed ) == limit ) {
						throw std::runtime_error( "comparator failed" );
					}
					return *lhs < *rhs;
				};
				bool threw = false;
				long compares_at_return = 0;
				{
					// Freed as soon as sort returns, so a task still running would touch freed
					// memory (reported when built with AddressSanitizer)
					auto values = std::vector<std::unique_ptr<int>>( );
					values.reserve( n );
					for( std::size_t i = 0; i < n; ++i ) {
						values.push_back( std::make_unique<int>( static_cast<int>( ( i * 31337 ) % 100000 ) ) );
					}
					try {
						daw::algorithm::parallel::sort( values.data( ), values.data( ) + values.size( ), ts, cmp );
					} catch( std::runtime_error const & ) { threw = true; }
					compares_at_return = compares->load( );
				}
				check( threw == ( compares_at_return > limit ),
				       "sort rethrows the comparator's exception",
				       static_cast<std::size_t>( limit ) );
				check( compares->load( ) == compares_at_return,
				       "no compares happen after sort returns",
				       static_cast<std::size_t>( limit ) );
			}
		}
	}
} // namespace

int main( ) {
	auto ts = daw::get_task_scheduler( );
	stable_sort_is_stable( ts );
	sort_matches_std_sort( ts );
	sort_move_only_type( ts );
	throwing_comparator( ts );
	if( g_failures != 0 ) {
		std::cerr << g_failures << " check(s) failed\n";
		return 1;
	}
	std::cout << "all sort correctness checks passed\n";
	return 0;
}
