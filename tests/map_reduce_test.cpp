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

#include <daw/daw_memory_mapped_file.h>
#include <daw/daw_semaphore.h>
#include <daw/daw_string_view.h>

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

std::vector<daw::string_view> find_newlines( daw::string_view str ) {
	std::vector<daw::string_view> results;
	if( str.empty( ) ) {
		return results;
	}
	auto pos = str.find( '\n' );
	while( pos != str.npos ) {
		results.push_back( str.substr( 0, pos - 1 ) );
		str.remove_prefix( pos + 1 );
		pos = str.find( '\n' );
	}
	results.push_back( str );
	return results;
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

std::vector<boost::optional<intmax_t>> parse_line( std::string line ) {
	std::vector<boost::optional<intmax_t>> result;
	daw::string_view view{line};
	auto comma_pos = find_commas( view );
	auto last_pos = 0;
	for( auto const &cur_pos : comma_pos ) {
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

auto parse_file( daw::string_view file_name ) {
	daw::filesystem::memory_mapped_file_t<char> mmf{file_name};

	if( !mmf ) {
		std::cerr << "Could not open input file '" << file_name << "' for reading\n";
		exit( EXIT_FAILURE );
	}
	auto lines = find_newlines( daw::string_view{mmf.data( ), mmf.size( )} );
	struct surplus_t {
		intmax_t value;
		bool has_value;

		surplus_t( ) : value{0}, has_value{false} {}
		surplus_t( intmax_t Value ) : value{Value}, has_value{true} {}

		explicit operator bool( ) const noexcept {
			return has_value;
		}
	};
	auto ts = daw::get_task_scheduler( );
	auto const mapper = daw::make_function_stream( []( daw::string_view line ) { return parse_line( line ); },
	                                         []( std::vector<boost::optional<intmax_t>> line_data ) {
		                                         auto fd = parsed_line_to_fd( line_data );
		                                         if( !fd ) {
			                                         return surplus_t{};
		                                         }
		                                         return surplus_t{fd->surplus};
	                                         } );

	return daw::algorithm::parallel::map_reduce( lines.cbegin( ), lines.cend( ),
	    [&ts, mapper]( daw::string_view line ) -> surplus_t {
		    auto result = mapper( line );
		    ts.blocking_on_waitable( result );
		    return result.get( );
	    },
	    []( surplus_t lhs, surplus_t rhs ) -> surplus_t {
		    if( lhs ) {
			    if( rhs ) {
				    lhs.value = std::min( lhs.value, rhs.value );
				    return lhs;
			    }
			    return lhs;
		    }
		    return rhs;
	    }, ts );
}

int main( int argc, char **argv ) {
	if( argc < 2 ) {
		std::cerr << "Must specify input file on commandline\n";
		exit( EXIT_FAILURE );
	}

	auto result = parse_file( argv[1] );

	std::cout << "Minimum surplus is ";
	if( result )
		std::cout << result.value << '\n';
	else {
		std::cout << "unknown\n";
	}
	return EXIT_SUCCESS;
}
