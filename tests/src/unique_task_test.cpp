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

// Tests for unique_task_t: callables stored inline and on the heap, moves, and
// notifying the task's semaphore

#include "daw/fs/task_scheduler.h"

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
	int g_failures = 0;

	void check( bool condition, char const *what ) {
		if( not condition ) {
			std::cerr << "FAILED: " << what << '\n';
			++g_failures;
		}
	}

	/// Counts live instances, so leaks and double destruction show up
	struct tracked {
		static inline int live = 0;
		int *calls;

		explicit tracked( int *c )
		  : calls( c ) {
			++live;
		}
		tracked( tracked const &other )
		  : calls( other.calls ) {
			++live;
		}
		tracked( tracked &&other ) noexcept
		  : calls( other.calls ) {
			++live;
		}
		tracked &operator=( tracked const & ) = delete;
		tracked &operator=( tracked && ) = delete;
		~tracked( ) {
			--live;
		}
		void operator( )( ) const {
			++*calls;
		}
	};

	/// Bigger than the inline buffer, so it must go on the heap
	struct big_task {
		tracked t;
		std::array<char, daw::unique_task_t::inline_size + 1> padding{ };
		void operator( )( ) const {
			t( );
		}
	};

	/// Small, but its move constructor may throw, so it must go on the heap
	struct throwing_move_task {
		tracked t;
		explicit throwing_move_task( int *c )
		  : t( c ) {}
		throwing_move_task( throwing_move_task const & ) = default;
		throwing_move_task( throwing_move_task &&other ) noexcept( false )
		  : t( other.t ) {}
		throwing_move_task &operator=( throwing_move_task const & ) = delete;
		throwing_move_task &operator=( throwing_move_task && ) = delete;
		~throwing_move_task( ) = default;
		void operator( )( ) const {
			t( );
		}
	};

	template<typename Task>
	void runs_and_moves( Task task, char const *what ) {
		int calls = 0;
		task.t.calls = &calls;
		{
			auto a = daw::unique_task_t( std::move( task ) );
			check( static_cast<bool>( a ), what );
			auto b = std::move( a );
			check( not a, "a moved from task is empty" );
			auto c = daw::unique_task_t( );
			c = std::move( b );
			check( not b, "a task moved from by assignment is empty" );
			c.execute( );
			c.execute( );
			check( calls == 2, what );
		}
	}
} // namespace

int main( ) {
	{
		int calls = 0;
		runs_and_moves( big_task{ tracked( &calls ) }, "heap stored task runs after moves" );
		runs_and_moves( throwing_move_task( &calls ), "throwing move task runs after moves" );
		check( tracked::live == 0, "every callable is destroyed exactly once" );
	}
	{
		// Inline: a closure holding a shared_ptr, like the scheduler's tasks
		auto counter = std::make_shared<int>( 0 );
		{
			auto a = daw::unique_task_t( [counter]( ) {
				++*counter;
			} );
			auto b = std::move( a );
			b.execute( );
		}
		check( *counter == 1, "inline task runs after a move" );
		check( counter.use_count( ) == 1, "inline task releases its captures" );
	}
	{
		// Assigning over a task destroys the callable it held
		int calls = 0;
		auto a = daw::unique_task_t( tracked( &calls ) );
		a = daw::unique_task_t( tracked( &calls ) );
		check( tracked::live == 1, "assignment destroys the previous callable" );
	}
	check( tracked::live == 0, "no callables leak" );
	{
		// The semaphore is notified after the task runs, even when it throws
		auto sem = daw::shared_cnt_sem( 1 );
		auto task = daw::unique_task_t(
		  []( ) {
			  throw std::runtime_error( "task failed" );
		  },
		  sem );
		sem.notify( ); // release the initial count, leaving the task's notifier
		check( not sem.try_wait( ), "semaphore waits for the task" );
		check( not task.try_wait( ), "task reports it has not run" );
		bool threw = false;
		try {
			task.execute( );
		} catch( std::runtime_error const & ) { threw = true; }
		check( threw, "the task's exception propagates out of execute" );
		check( sem.try_wait( ), "semaphore is notified even though the task threw" );
	}
	{
		// A task without a semaphore never reports that it has finished
		auto task = daw::unique_task_t( [] {} );
		check( not task.try_wait( ), "task without a semaphore reports false" );
		task.execute( );
	}
	if( g_failures != 0 ) {
		std::cerr << g_failures << " check(s) failed\n";
		return 1;
	}
	std::cout << "all unique_task_t checks passed\n";
	return 0;
}
