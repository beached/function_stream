// The MIT License (MIT)
//
// Copyright (c) 2016 Darrell Wright
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

#pragma once

#include <iterator>
#include <memory>

#include <daw/daw_poly_vector.h>

#include "function_stream.h"

namespace daw {
	namespace algorithm {
		struct task_group_detail_t: public std::enable_shared_from_this<task_group_detail_t> {
			daw::poly_vector_t<daw::future_result_base_t> items;

			void wait( ) const {
				for( auto const & result : items ) {
					result->wait( );
				}
			}

			bool try_wait( ) const {
				for( auto const & result : items ) {
					if( !result->try_wait( ) ) {
						return false;
					}
				}
				return true;
			}

			explicit operator bool( ) const {
				return try_wait( );
			}

			std::weak_ptr<task_group_detail_t const> get_weak_ptr( ) const {
				return this->shared_from_this( );
			}

			std::weak_ptr<task_group_detail_t> get_weak_ptr( ) {
				return this->shared_from_this( );
			}
		};	// task_group_detail_t

		template<typename... Functions>
		struct task_group_t {
			using functions_t = daw::function_stream<Functions...>;
			functions_t functions;
			
			std::shared_ptr<task_group_detail_t> results;

			template<typename OnComplete, typename... Args>
			void operator( )( OnComplete on_complete, Args&&... args ) {
				results.items.push_back( functions( std::forward<Args>( args )... ) );
				std::function<void( )> check_status;
				check_status = [check_status, on_complete]
			}


		};

		template<typename Function>
		auto make_task_group( Function on_competion ) {
			return task_group_t<Function>{ };
		}



		template<size_t idx, typename RandomIterator, typename Compare>
		auto ms_impl( RandomIterator first, RandomIterator last, Compare cmp ) {
			using value_type = typename std::iterator_traits<RandomIterator>::value_type;

			auto results = make_task_group( []( auto const & r ) {
				

			} );
			auto const count = std::distance( f, l );
			auto const chunk_size = (64 * 1024) / sizeof( value_type );
			
			for( size_t n=0; n<chunk_size; ++n ) {
				results.push_back( [=]( RandomIterator f, RandomIterator l ) {

				} );
			// Using a WAG of 64k for minimum size to leave the CPU
			if( sizeof( value_type )*count < 64*1024 ) {
				std::sort( first, last, cmp );
				return;
			}
			auto const mid = std::next( first, count / 2 );
			
				
		}

		template<size_t idx, typename RandomIterator>
		void ms_impl( RandomIterator first, RandomIterator last ) {
			using value_type = typename std::iterator_traits<RandomIterator>::value_type;
			ms_impl( first, last, std::less<value_type>{ } ); 
		}

		template<typename RandomIterator, typename Compare>
		void parallel_merge_sort( RandomIterator first, RandomIterator last ) {
			std::function<daw::future_result_t<std::pair<RandomIterator, RandomIterator>>( size_t, RandomIterator, RandomIterator )> f;
			f = [compare]( size_t idx, RandomIterator f, RandomIterator l ) -> daw::future_result_t<std::pair<RandomIterator, RandomIterator>> {
				auto const sz = std::distance( f, l );
				using value_t = std::iterator_traits<RandomIterator>::value_type;
				if( sz*sizeof(value_t> < 64*1024 ) {
						std::sort( f, l );
				if( std::distance( f, l ) > 1 ) {
					
				}
				return std::make_pair( f, l );
			};

		}
	}	// namespace algorithm
}    // namespace daw

