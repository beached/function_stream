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

#include <boost/optional.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <daw/daw_string_view.h>
#include <daw/daw_semaphore.h>

#include "algorithms.h"
#include "function_stream.h"

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

std::vector<size_t> find_commas( daw::string_view line ) {
	bool in_quote = false;
	std::vector<size_t> results;
	for( auto it = line.begin( ); it != line.end( ); ++it ) {
		switch( *it ) {
		case '"':
			in_quote = !in_quote;
			break;
		case ',':
			if( !in_quote ) {
				results.push_back( static_cast<size_t>(std::distance( line.begin( ), it ) ) );
			}
			break;
		default:
			break;
		}
	}
	results.push_back( line.npos );
	return results;
}

template<typename Char>
constexpr bool is_number( Char c ) noexcept {
	return '0' <= c && c <= '9';
}

boost::optional<intmax_t> parse_int( daw::string_view str ) {
	if( str.empty() ) {
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
	if( it == str.end( ) || !is_number( *it )) {
		return boost::none;
	}
	intmax_t result = (*it++) - '0';
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

std::vector<boost::optional<intmax_t>> parse_line( std::string line ) {
	std::vector<boost::optional<intmax_t>> result;
	daw::string_view view{line};
	auto comma_pos = find_commas( view );
	auto last_pos = 0;
	for( auto const & cur_pos: comma_pos ) {
		result.push_back( parse_int( view.substr( last_pos, cur_pos - last_pos ) ) );
		last_pos = cur_pos + 1;
	}
	return result;
}

boost::optional<file_data> parsed_line_to_fd( std::vector<boost::optional<intmax_t>> parsed_line ) {
	if( parsed_line.size( ) >= 4 && parsed_line[0] && parsed_line[1] && parsed_line[2] && parsed_line[3] ) {
		return file_data{*parsed_line[0], *parsed_line[1], *parsed_line[2], *parsed_line[3]};
	}
	return boost::none;
}

size_t count_lines( std::ifstream & ifs ) {
	auto orig_pos = ifs.tellg( );
	ifs.clear( );
	ifs.seekg( 0 );
	auto result = static_cast<size_t>(
	    std::count( std::istreambuf_iterator<char>( ifs ), std::istreambuf_iterator<char>( ), '\n' ) );
	ifs.clear( );
	ifs.seekg( orig_pos );
	return result;
}

auto parse_file( daw::string_view file_name ) {
	std::ifstream in_file{file_name.data( )};

	if( !in_file ) {
		std::cerr << "Could not open input file '" << file_name << "' for reading\n";
		exit( EXIT_FAILURE );
	}
	std::vector<file_data> result;

	for( std::string line; std::getline( in_file, line ); ) {
		auto fd = parsed_line_to_fd( parse_line( line ) );
		if( fd ) {
			result.push_back( *fd );
		}
	}
	return result;
}

int main( int argc, char**argv ) {
	if( argc < 2 ) { 
		std::cerr << "Must specify input file on commandline\n";
		exit( EXIT_FAILURE );
	}
	auto fd = parse_file( argv[1] );

	struct {
		constexpr intmax_t operator( )( intmax_t value ) const noexcept {
			return value;
		}
		constexpr intmax_t operator( )( file_data const & value ) const noexcept {
			return value.surplus;
		}
	} mapper;

	auto const reducer = []( intmax_t lhs, intmax_t rhs ) noexcept {
		return std::min( lhs, rhs );
	};

	auto result = daw::algorithm::parallel::map_reduce( fd.cbegin( ), fd.cend( ), mapper, reducer );

	std::cout << "Minimum surplus is " << result << '\n';
	return EXIT_SUCCESS;
}

