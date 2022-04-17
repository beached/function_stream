// The MIT License (MIT)
//
// Copyright (c) 2017-2019 Darrell Wright
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

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <daw/daw_benchmark.h>
#include <daw/daw_memory_mapped_file.h>
#include <daw/daw_span.h>
#include <daw/daw_string_view.h>
#include <daw/parallel/daw_semaphore.h>

#include "daw/fs/algorithms.h"
#include "daw/fs/function_stream.h"
#include "daw/fs/message_queue.h"

template<size_t max_find, typename Function>
constexpr void find_commas( daw::string_view line, Function on_commas ) {
	std::array<size_t, max_find> result{};
	bool in_quote = false;
	size_t count = 0;
	for( auto it = line.cbegin( ); it != line.cend( ) && count <= max_find;
	     ++it ) {
		switch( *it ) {
		case '"':
			in_quote = !in_quote;
			break;
		case ',':
			if( !in_quote ) {
				result[count++] =
				  static_cast<size_t>( std::distance( line.cbegin( ), it ) );
			}
			break;
		default:
			break;
		}
	}
	if( count > max_find ) {
		on_commas( DAW_MOVE( result ) );
	}
}

template<typename T, typename Emitter>
constexpr void find_newlines( daw::span<T> str, Emitter emitter ) {
	auto const last = str.cend( );
	auto last_pos = str.cbegin( );
	str.remove_prefix( );
	while( !str.empty( ) ) {
		if( str.front( ) == '\n' ) {
			emitter( daw::make_string_view_it( last_pos, str.cbegin( ) ) );
			str.remove_prefix( );
			last_pos = str.data( );
			continue;
		}
		str.remove_prefix( );
	}
	if( last_pos < last ) {
		emitter( daw::make_string_view_it( last_pos, last ) );
	}
}

template<typename Char>
constexpr bool is_number( Char c ) noexcept {
	return ( static_cast<uint32_t>( c ) - '0' ) < 10;
}

template<typename CharT>
constexpr intmax_t to_number( CharT val ) noexcept {
	return val - '0';
}

template<typename Function>
constexpr void parse_int( daw::string_view str, Function on_number ) {
	while( !str.empty( ) &&
	       !( is_number( str.front( ) ) || str.front( ) == '-' ) ) {
		str.remove_prefix( );
	}
	if( str.empty( ) ) {
		return;
	}
	bool is_negative = str.front( ) == '-';
	if( is_negative ) {
		str.remove_prefix( );
	}
	if( str.empty( ) || !is_number( str.front( ) ) ) {
		return;
	}
	intmax_t result = to_number( str.pop_front( ) );
	while( !str.empty( ) && is_number( str.front( ) ) ) {
		result *= 10;
		result += to_number( str.pop_front( ) );
		if( !str.empty( ) && str.front( ) == ',' ) {
			str.remove_prefix( );
		}
	}
	if( is_negative ) {
		result *= -1;
	}
	on_number( result );
}

template<typename Function>
inline void parse_line( daw::string_view line, Function value_cb ) {
	find_commas<4>( line, [value_cb, line]( std::array<size_t, 4> comma_pos ) {
		auto const first = comma_pos[2] + 1;
		parse_int( line.substr( first, comma_pos[3] - first ), value_cb );
	} );
}

template<typename Range>
daw::future_result_t<intmax_t> parse_file( Range str, daw::task_scheduler ts ) {
	daw::parallel::spsc_msg_queue_t<daw::string_view> str_lines_result{
	  daw::parallel::use_autosize{}};

	ts.add_task( [str = DAW_MOVE( str ), str_lines_result]( ) mutable {
		auto const at_exit = daw::on_scope_exit(
		  [str_lines_result]( ) mutable { str_lines_result.notify_completed( ); } );

		find_newlines( str, [str_lines_result = DAW_MOVE( str_lines_result )](
		                      daw::string_view line ) mutable {
			while( !str_lines_result.send( line ) ) {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for( 1ns );
			}
		} );
	} );

	daw::parallel::spsc_msg_queue_t<intmax_t> parsed_lines_result{
	  daw::parallel::use_autosize{}};

	ts.add_task( [str_lines_result, parsed_lines_result]( ) mutable {
		auto const at_exit = daw::on_scope_exit( [parsed_lines_result]( ) mutable {
			parsed_lines_result.notify_completed( );
		} );
		while( str_lines_result.has_more( ) ) {
			daw::string_view cur_line;
			while( !str_lines_result.receive( cur_line ) ) {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for( 1ns );
			}
			parse_line( cur_line, [&parsed_lines_result]( intmax_t v ) {
				parsed_lines_result.send( v );
			} );
		}
	} );

	auto result = daw::make_future_result( [parsed_lines_result]( ) mutable {
		intmax_t cur_min = std::numeric_limits<intmax_t>::max( );
		intmax_t cur_value = std::numeric_limits<intmax_t>::max( );
		while( parsed_lines_result.has_more( ) ) {
			while( !parsed_lines_result.receive( cur_value ) ) {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for( 1ns );
			}
			cur_min = std::min( cur_min, cur_value );
		}
		return cur_min;
	} );

	return result;
}

int main( int argc, char **argv ) {
	if( argc < 2 ) {
		std::cerr << "Must specify input file on commandline\n";
		exit( EXIT_FAILURE );
	}

	daw::filesystem::memory_mapped_file_t<char> mmf{argv[1]};
	daw::exception::daw_throw_on_false( mmf,
	                                    "Could not open input file for reading" );

	auto time1 = std::numeric_limits<double>::max( );
	auto time2 = std::numeric_limits<double>::max( );
	auto const sz = mmf.size( );
	{
		auto view = daw::span( mmf.data( ), sz );
		auto result = parse_file( view, daw::get_task_scheduler( ) );
		time1 = daw::benchmark( [&result]( ) { result.wait( ); } );

		std::cout << "File test\n";
		std::cout << "Processed " << daw::utility::to_bytes_per_second( sz )
		          << " bytes in " << daw::utility::format_seconds( time1, 3 )
		          << " seconds\n";
		std::cout << "Speed " << daw::utility::to_bytes_per_second( sz, time1 )
		          << "/s\n";
		std::cout << "Minimum surplus is ";
		std::cout << result.get( ) << '\n';
	}

	{
		std::vector<char> mem_data;
		mem_data.reserve( sz );
		std::copy( mmf.cbegin( ), mmf.cend( ), std::back_inserter( mem_data ) );
		mmf.close( );
		auto view = daw::span( mem_data.data( ), mem_data.size( ) );
		auto result = parse_file( view, daw::get_task_scheduler( ) );
		time2 = daw::benchmark( [&result]( ) { result.wait( ); } );

		std::cout << "Memory test\n";
		std::cout << "Processed " << daw::utility::to_bytes_per_second( sz )
		          << " bytes in " << daw::utility::format_seconds( time2, 3 )
		          << " seconds\n";
		std::cout << "Speed " << daw::utility::to_bytes_per_second( sz, time2 )
		          << "/s\n";
		std::cout << "Minimum surplus is ";
		std::cout << result.get( ) << '\n';
	}

	return EXIT_SUCCESS;
}
