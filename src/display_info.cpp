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

#include <chrono>
#include <cstdint>
#include <date/date.h>
#include <iostream>
#include <iomanip>
#include <thread>

#include <daw/daw_string_view.h>

#include "display_info.h"

namespace {
	template<typename T>
	double calc_speedup( T seq_time, T par_time ) {
		static double const N = std::thread::hardware_concurrency( );
		return 100.0 * ( ( seq_time / N ) / par_time );
	}
} // namespace

void display_info( double seq_time, double par_time, double count, size_t bytes,
                   daw::string_view label ) {
	using namespace std::chrono;
	using namespace date;

	auto const make_seconds = []( double t, double c ) {
		auto val = ( t / c ) * 1000000000000.0;

		if( val < 1000 ) {
			return std::to_string( static_cast<uint64_t>( val ) ) + "ps";
		}
		val /= 1000.0;
		if( val < 1000 ) {
			return std::to_string( static_cast<uint64_t>( val ) ) + "ns";
		}
		val /= 1000.0;
		if( val < 1000 ) {
			return std::to_string( static_cast<uint64_t>( val ) ) + "µs";
		}
		val /= 1000.0;
		if( val < 1000 ) {
			return std::to_string( static_cast<uint64_t>( val ) ) + "ms";
		}
		val /= 1000.0;
		return std::to_string( static_cast<uint64_t>( val ) ) + "s";
	};

	auto const mbs = [count, bytes]( double t ) {
		using result_t = double;
		std::stringstream ss;
		ss << std::setprecision( 1 ) << std::fixed;
		auto val = ( count * static_cast<double>( bytes ) ) / t;
		if( val < 1024 ) {
			ss << ( static_cast<result_t>( val * 100.0 ) / 100 ) << "bytes";
			return ss.str( );
		}
		val /= 1024.0;
		if( val < 1024 ) {
			ss << ( static_cast<result_t>( val * 100.0 ) / 100 ) << "KB";
			return ss.str( );
		}
		val /= 1024.0;
		if( val < 1024 ) {
			ss << ( static_cast<result_t>( val * 100.0 ) / 100 ) << "MB";
			return ss.str( );
		}
		val /= 1024.0;
		ss << ( static_cast<result_t>( val * 100.0 ) / 100 ) << "GB";
		return ss.str( );
	};

	std::cout << std::setprecision( 1 ) << std::fixed << label << ": size->"
	          << static_cast<uint64_t>( count ) << " " << mbs( 1 ) << " %max->"
	          << calc_speedup( seq_time, par_time ) << "("
	          << ( seq_time / par_time ) << "/"
	          << std::thread::hardware_concurrency( ) << "X) par_total->"
	          << make_seconds( par_time, 1 ) << " par_item->"
	          << make_seconds( par_time, count ) << " throughput->"
	          << mbs( par_time ) << "/s seq_total->"
	          << make_seconds( seq_time, 1 ) << " seq_item->"
	          << make_seconds( seq_time, count ) << " throughput->"
	          << mbs( seq_time ) << "/s \n";
}
