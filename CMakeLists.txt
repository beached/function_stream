cmake_minimum_required( VERSION 2.8.12 )

project( function_stream_prj )

find_package( Boost 1.58.0 COMPONENTS system iostreams filesystem regex unit_test_framework REQUIRED )

enable_testing( )
add_definitions( -DBOOST_ALL_NO_LIB )

find_package( Threads REQUIRED )

set( CMAKE_CXX_STANDARD 17 CACHE STRING "The C++ standard whose features are requested.")
add_definitions( -DBOOST_TEST_DYN_LINK -DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK )

if( ${CMAKE_CXX_COMPILER_ID} STREQUAL 'MSVC' )
	add_definitions( -DNOMINMAX -DD_WIN32_WINNT=0x0601 )
else( )
	if( ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang" )
		add_compile_options( -march=native -pthread -Weverything -Wno-c++98-compat -Wno-covered-switch-default -Wno-padded -Wno-exit-time-destructors -Wno-c++98-compat-pedantic -Wno-unused-parameter -Wno-missing-noreturn -Wno-missing-prototypes -Wno-disabled-macro-expansion -Wno-missing-braces)
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -DDEBUG")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3")
	elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" )
		add_compile_options(-march=native -pthread -Wall -Wno-deprecated-declarations -Wduplicated-cond -Wlogical-op -Wnull-dereference -Wold-style-cast -Wshadow)
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -DDEBUG")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3")
	endif( )
endif( )

set( DEP_ROOT_DIR_BASE "${CMAKE_SOURCE_DIR}/dependent_projects/build" )
if( CMAKE_BUILD_TYPE EQUAL "Debug" )
	set( DEP_ROOT_DIR "${DEP_ROOT_DIR_BASE}/debug" )
elseif( CMAKE_BUILD_TYPE EQUAL "Release" )
	set( DEP_ROOT_DIR "${DEP_ROOT_DIR_BASE}/release" )
endif( )

message( "Building for: ${CMAKE_BUILD_TYPE}" )
if( EXISTS "${DEP_ROOT_DIR}" )
	message( "${DEP_ROOT_DIR}" )
	message( "using dependency folder" )
	add_custom_target( dependency_stub )
	include_directories( SYSTEM "${DEP_ROOT_DIR}/install/include" )
	link_directories( "${DEP_ROOT_DIR}/install/lib" )
else( )
	message( "downloading and building deps" )
	include( "${CMAKE_SOURCE_DIR}/dependent_projects/dependencies.txt" )
	add_library( dependency_stub STATIC "${CMAKE_SOURCE_DIR}/dependent_projects/stub.cpp" )
	add_dependencies( dependency_stub header_libraries_prj date_prj )
endif( )

set( HEADER_FOLDER "include" )
set( SOURCE_FOLDER "src" )
set( TEST_FOLDER "tests" )

include_directories( SYSTEM "${CMAKE_BINARY_DIR}/install/include" )
include_directories( ${HEADER_FOLDER} )

include_directories( SYSTEM ${Boost_INCLUDE_DIRS} )

link_directories( ${Boost_LIBRARY_DIRS} )

set( FUNCTION_STREAM_HEADER_FILES
	${HEADER_FOLDER}/daw/fs/algorithms.h
	${HEADER_FOLDER}/daw/fs/function_stream.h
	${HEADER_FOLDER}/daw/fs/future_result.h
	${HEADER_FOLDER}/daw/fs/iterator_range.h
	${HEADER_FOLDER}/daw/fs/package.h
	${HEADER_FOLDER}/daw/fs/message_queue.h
	${HEADER_FOLDER}/daw/fs/task_scheduler.h
	${HEADER_FOLDER}/daw/fs/impl/algorithms_impl.h
	${HEADER_FOLDER}/daw/fs/impl/concept_checks.h
	${HEADER_FOLDER}/daw/fs/impl/function_stream_impl.h
	${HEADER_FOLDER}/daw/fs/impl/future_result_impl.h
	${HEADER_FOLDER}/daw/fs/impl/task_scheduler_impl.h
)

set( FUNCTION_STREAM_SOURCE_FILES
	${SOURCE_FOLDER}/future_result.cpp
)

set( TASK_SCHEDULER_HEADER_FILES
	${HEADER_FOLDER}/daw/fs/task_scheduler.h
)

set( TASK_SCHEDULER_SOURCE_FILES
	${SOURCE_FOLDER}/task_scheduler.cpp
)

add_definitions( -DBOOST_TEST_DYN_LINK -DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK )

#set( CMAKE_CTEST_COMMAND ctest -V )
add_custom_target( check COMMAND ${CMAKE_CTEST_COMMAND} )

add_library( task_scheduler_lib ${TASK_SCHEDULER_HEADER_FILES} ${TASK_SCHEDULER_SOURCE_FILES} )
target_link_libraries( task_scheduler_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_dependencies( task_scheduler_lib dependency_stub )

add_library( function_stream_lib ${FUNCTION_STREAM_HEADER_FILES} ${TASK_SCHEDULER_HEADER_FILES} ${FUNCTION_STREAM_SOURCE_FILES} )
target_link_libraries( function_stream_lib task_scheduler_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_dependencies( function_stream_lib dependency_stub )

add_library( display_info_lib ${HEADER_FOLDER}/display_info.h ${SOURCE_FOLDER}/display_info.cpp )
target_link_libraries( display_info_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_dependencies( display_info_lib dependency_stub )

add_executable( task_scheduler_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/task_scheduler_test.cpp )
target_link_libraries( task_scheduler_test_bin task_scheduler_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( task_scheduler_test task_scheduler_test_bin )
add_dependencies( check task_scheduler_test_bin )

add_executable( function_stream_test_bin EXCLUDE_FROM_ALL ${FUNCTION_STREAM_HEADER_FILE} ${TASK_SCHEDULER_HEADER_FILES} ${TEST_FOLDER}/function_stream_test.cpp )
target_link_libraries( function_stream_test_bin task_scheduler_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( function_stream_test function_stream_test_bin )
add_dependencies( check function_stream_test_bin )

add_executable( future_result_test_bin EXCLUDE_FROM_ALL ${FUNCTION_STREAM_HEADER_FILES} ${TASK_SCHEDULER_HEADER_FILES} ${TEST_FOLDER}/future_result_test.cpp )
target_link_libraries( future_result_test_bin task_scheduler_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( future_result_test future_result_test_bin )
add_dependencies( check future_result_test_bin )

add_executable( function_composition_test_bin EXCLUDE_FROM_ALL ${FUNCTION_STREAM_HEADER_FILES} ${TASK_SCHEDULER_HEADER_FILES} ${TEST_FOLDER}/function_composition_test.cpp )
target_link_libraries( function_composition_test_bin task_scheduler_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( function_composition_test function_composition_test_bin )
add_dependencies( check function_composition_test_bin )

add_executable( function_composition_test_bin2 EXCLUDE_FROM_ALL ${FUNCTION_STREAM_HEADER_FILES} ${TASK_SCHEDULER_HEADER_FILES} ${TEST_FOLDER}/function_composition_test2.cpp )
target_link_libraries( function_composition_test_bin2 task_scheduler_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( function_composition_test2 function_composition_test_bin2 )
add_dependencies( check function_composition_test_bin2 )

add_executable( algorithms_for_each_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_for_each_test.cpp )
target_link_libraries( algorithms_for_each_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_for_each_test algorithms_for_each_test_bin )
add_dependencies( check algorithms_for_each_test_bin )

add_executable( algorithms_fill_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_fill_test.cpp )
target_link_libraries( algorithms_fill_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_fill_test algorithms_fill_test_bin )
add_dependencies( check algorithms_fill_test_bin )

add_executable( algorithms_sort_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_sort_test.cpp )
target_link_libraries( algorithms_sort_test_bin task_scheduler_lib display_info_lib function_stream_lib ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_sort_test algorithms_sort_test_bin )
add_dependencies( check algorithms_sort_test_bin )

add_executable( algorithms_stable_sort_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_stable_sort_test.cpp )
target_link_libraries( algorithms_stable_sort_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_stable_sort_test algorithms_stable_sort_test_bin )
add_dependencies( check algorithms_stable_sort_test_bin )

add_executable( algorithms_reduce_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_reduce_test.cpp )
target_link_libraries( algorithms_reduce_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_reduce_test algorithms_reduce_test_bin )
add_dependencies( check algorithms_reduce_test_bin )

add_executable( algorithms_min_max_element_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_min_max_element_test.cpp )
target_link_libraries( algorithms_min_max_element_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_min_max_element_test algorithms_min_max_element_test_bin )
add_dependencies( check algorithms_min_max_element_test_bin )

add_executable( algorithms_transform_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_transform_test.cpp )
target_link_libraries( algorithms_transform_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_transform_test algorithms_transform_test_bin )
add_dependencies( check algorithms_transform_test_bin )

add_executable( algorithms_map_reduce_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_map_reduce_test.cpp )
target_link_libraries( algorithms_map_reduce_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_map_reduce_test algorithms_map_reduce_test_bin )
add_dependencies( check algorithms_map_reduce_test_bin )

add_executable( algorithms_scan_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_scan_test.cpp )
target_link_libraries( algorithms_scan_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_scan_test algorithms_scan_test_bin )
add_dependencies( check algorithms_scan_test_bin )

add_executable( algorithms_equal_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_equal_test.cpp )
target_link_libraries( algorithms_equal_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_equal_test algorithms_equal_test_bin )
add_dependencies( check algorithms_equal_test_bin )

add_executable( algorithms_count_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_count_test.cpp )
target_link_libraries( algorithms_count_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_count_test algorithms_count_test_bin )
add_dependencies( check algorithms_count_test_bin )

add_executable( algorithms_chunked_for_each_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_chunked_for_each_test.cpp )
target_link_libraries( algorithms_chunked_for_each_test_bin task_scheduler_lib display_info_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_chunked_for_each_test algorithms_chunked_for_each_test_bin )
add_dependencies( check algorithms_chunked_for_each_test_bin )

add_executable( map_reduce_test_bin EXCLUDE_FROM_ALL ${FUNCTION_STREAM_HEADER_FILES} ${TASK_SCHEDULER_HEADER_FILES} ${TEST_FOLDER}/map_reduce_test.cpp )
target_link_libraries( map_reduce_test_bin task_scheduler_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( map_reduce_test map_reduce_test_bin )
add_dependencies( check map_reduce_test_bin )

install( TARGETS function_stream_lib task_scheduler_lib DESTINATION lib )
install( DIRECTORY ${HEADER_FOLDER}/daw DESTINATION include )

