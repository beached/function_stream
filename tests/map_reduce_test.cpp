// The MIT License (MIT)
//
// Copyright (c) 2017 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <algorithm>
#include <boost/optional.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <daw/daw_benchmark.h>
#include <daw/daw_memory_mapped_file.h>
#include <daw/daw_semaphore.h>
#include <daw/daw_string_view.h>

#include "algorithms.h"
#include "function_stream.h"
#include "message_queue.h"

constexpr daw::string_view trim_quotes( daw::string_view sv ) noexcept {
	if( sv.empty( ) ) {
		return sv;
	}
	if( sv.front( ) == '"' ) {
		sv.remove_prefix( );
	}
	if( sv.back( ) == '"' ) {
		sv.remove_suffix( );
	}
	return sv;
}

struct file_data {
	intmax_t year;
	intmax_t receipts;
	intmax_t outlays;
	intmax_t surplus;
};

template<size_t max_find = std::numeric_limits<size_t>::max( )>
std::vector<size_t> find_commas( daw::string_view line ) {
	bool in_quote = false;
	std::vector<size_t> results;
	for( auto it = line.begin( ); it != line.end( ) && results.size( ) <= max_find; ++it ) {
		switch( *it ) {
		case '"':
			in_quote = !in_quote;
			break;
		case ',':
			if( !in_quote ) {
				results.push_back( static_cast<size_t>( std::distance( line.begin( ), it ) ) );
			}
			break;
		default:
			break;
		}
	}
	results.push_back( line.npos );
	return results;
}

template<typename Emitter>
void find_newlines( daw::string_view str, Emitter emitter ) {
	auto pos = str.find( '\n' );
	while( pos != str.npos ) {
		emitter( str.substr( 0, pos - 1 ) );
		str.remove_prefix( pos + 1 );
		pos = str.find( '\n' );
	}
	if( !str.empty( ) ) {
		emitter( str );
	}
}

template<typename Char>
constexpr bool is_number( Char c ) noexcept {
	return '0' <= c && c <= '9';
}

boost::optional<intmax_t> parse_int( daw::string_view str ) {
	if( str.empty( ) ) {
		return boost::none;
	}
	auto it = str.begin( );
	for( ; it != str.end( ); ++it ) {
		if( is_number( *it ) || *it == '-' ) {
			break;
		}
	}
	if( it == str.end( ) ) {
		return boost::none;
	}
	bool is_negative = *it == '-';
	if( is_negative ) {
		++it;
	}
	if( it == str.end( ) || !is_number( *it ) ) {
		return boost::none;
	}
	intmax_t result = ( *it++ ) - '0';
	for( ; it != str.end( ); ++it ) {
		if( is_number( *it ) ) {
			result *= 10;
			result += *it - '0';
		}
	}
	if( is_negative ) {
		result *= -1;
	}
	return result;
}

template<typename Function>
void parse_line( daw::string_view line, Function value_cb ) {
	std::vector<boost::optional<intmax_t>> result;
	auto comma_pos = find_commas<4>( line );

	if( comma_pos.size( ) >= 4 ) {
		auto const p1 = comma_pos[2] + 1;
		auto const p2 = comma_pos[3];
		auto const sz = p2 - p1;
		value_cb( parse_int( line.substr( comma_pos[2] + 1, sz ) ) );
	}
}

boost::optional<file_data> parsed_line_to_fd( std::vector<boost::optional<intmax_t>> parsed_line ) {
	if( parsed_line.size( ) >= 4 && parsed_line[0] && parsed_line[1] && parsed_line[2] && parsed_line[3] ) {
		auto result = file_data{*parsed_line[0], *parsed_line[1], *parsed_line[2], *parsed_line[3]};
		return result;
	}
	return boost::none;
}

daw::future_result_t<intmax_t> parse_file( daw::string_view str, daw::task_scheduler ts ) {
	daw::parallel::spsc_msg_queue_t<daw::string_view> str_lines_result{1024};

	ts.add_task( [str, str_lines_result]( ) mutable {
		auto const at_exit = daw::on_scope_exit( [str_lines_result]( ) mutable { str_lines_result.notify_completed( ); } );

		find_newlines( str, [str_lines_result = std::move( str_lines_result )]( daw::string_view line ) mutable {
			while( !str_lines_result.send( line ) ) {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for( 1ns );
			}
		} );
	} );

	daw::parallel::spsc_msg_queue_t<intmax_t> parsed_lines_result{128};

	ts.add_task( [str_lines_result, parsed_lines_result]( ) mutable {
		auto const at_exit =
		    daw::on_scope_exit( [parsed_lines_result]( ) mutable { parsed_lines_result.notify_completed( ); } );
		while( str_lines_result.has_more( ) ) {
			daw::string_view cur_line{};
			while( !str_lines_result.receive( cur_line ) ) {
				using namespace std::chrono_literals;
				std::this_thread::sleep_for( 1ns );
			}
			parse_line( cur_line, [&]( auto const &v ) {
				if( v ) {
					parsed_lines_result.send( *v );
				}
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
			if( cur_value < cur_min ) {
				cur_min = cur_value;
			}
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
	daw::exception::daw_throw_on_false( mmf, "Could not open input file for reading" );
	auto result = parse_file( daw::string_view{mmf.data( ), mmf.size( )}, daw::get_task_scheduler( ) );

	auto time = daw::benchmark( [&result]( ) { result.wait( ); } );
	std::cout << "Processed " << daw::utility::to_bytes_per_second( mmf.size( ) ) << " bytes in "
	          << daw::utility::format_seconds( time, 3 ) << " seconds\n";
	std::cout << "Speed " << daw::utility::to_bytes_per_second( mmf.size( ), time ) << "/s\n";
	std::cout << "Minimum surplus is ";
	std::cout << result.get( ) << '\n';
	return EXIT_SUCCESS;
}
