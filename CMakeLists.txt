cmake_minimum_required( VERSION 2.8.12 )

project( function_stream_prj )

find_package( Boost 1.58.0 COMPONENTS system iostreams filesystem regex unit_test_framework REQUIRED )

enable_testing( )
add_definitions( -DBOOST_ALL_NO_LIB )

find_package( Threads REQUIRED )

if( ${CMAKE_CXX_COMPILER_ID} STREQUAL 'MSVC' )
	add_compile_options( -D_WIN32_WINNT=0x0601 /std:c++latest )
else( )
	if( ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang" )
		add_compile_options(-std=c++14 -march=native -pthread -pedantic -Weverything -Wno-c++98-compat -Wno-covered-switch-default -Wno-padded -Wno-exit-time-destructors -Wno-c++98-compat-pedantic -Wno-unused-parameter -Wno-missing-noreturn -Wno-missing-prototypes -Wno-disabled-macro-expansion)
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
	elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" )
		add_compile_options(-std=c++14 -march=native -pthread -Wall -Wno-noexcept-type -Wno-deprecated-declarations -Wduplicated-cond -Wlogical-op -Wnull-dereference -Wold-style-cast -Wshadow)
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
	endif( )
endif( )

include_directories( SYSTEM "${CMAKE_SOURCE_DIR}/dependent_projects/build/install/include" )
link_directories( "${CMAKE_SOURCE_DIR}/dependent_projects/build/install/lib" )

set( HEADER_FOLDER "include" )
set( SOURCE_FOLDER "src" )
set( TEST_FOLDER "tests" )

include_directories( SYSTEM "${CMAKE_BINARY_DIR}/install/include" )
include_directories( ${HEADER_FOLDER} )

include_directories( SYSTEM ${Boost_INCLUDE_DIRS} )

link_directories( ${Boost_LIBRARY_DIRS} )

set( FUNCTION_STREAM_HEADER_FILES
	${HEADER_FOLDER}/algorithms.h
	${HEADER_FOLDER}/function_stream.h
	${HEADER_FOLDER}/future_result.h
	${HEADER_FOLDER}/iterator_range.h
	${HEADER_FOLDER}/package.h
	${HEADER_FOLDER}/message_queue.h
	${HEADER_FOLDER}/task_scheduler.h
	${HEADER_FOLDER}/impl/algorithms_impl.h
	${HEADER_FOLDER}/impl/concept_checks.h
	${HEADER_FOLDER}/impl/counting_semaphore.h
	${HEADER_FOLDER}/impl/function_stream_impl.h
	${HEADER_FOLDER}/impl/future_result_impl.h
	${HEADER_FOLDER}/impl/task_scheduler_impl.h
)

set( FUNCTION_STREAM_SOURCE_FILES
	${SOURCE_FOLDER}/future_result.cpp
)

set( TASK_SCHEDULER_HEADER_FILES
	${HEADER_FOLDER}/task_scheduler.h
)

set( TASK_SCHEDULER_SOURCE_FILES
	${SOURCE_FOLDER}/task_scheduler.cpp
)

add_definitions( -DBOOST_TEST_DYN_LINK -DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK )

#set( CMAKE_CTEST_COMMAND ctest -V )
add_custom_target( check COMMAND ${CMAKE_CTEST_COMMAND} )

add_library( task_scheduler_lib ${TASK_SCHEDULER_HEADER_FILES} ${TASK_SCHEDULER_SOURCE_FILES} )
target_link_libraries( task_scheduler_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )

add_library( function_stream_lib ${FUNCTION_STREAM_HEADER_FILES} ${TASK_SCHEDULER_HEADER_FILES} ${FUNCTION_STREAM_SOURCE_FILES} )
target_link_libraries( function_stream_lib task_scheduler_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )

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

add_executable( algorithms_test_bin EXCLUDE_FROM_ALL ${TEST_FOLDER}/algorithms_test.cpp )
target_link_libraries( algorithms_test_bin task_scheduler_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( algorithms_test algorithms_test_bin )
add_dependencies( check algorithms_test_bin )

add_executable( map_reduce_test_bin EXCLUDE_FROM_ALL ${FUNCTION_STREAM_HEADER_FILES} ${TASK_SCHEDULER_HEADER_FILES} ${TEST_FOLDER}/map_reduce_test.cpp )
target_link_libraries( map_reduce_test_bin task_scheduler_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_test( map_reduce_test map_reduce_test_bin )
add_dependencies( check map_reduce_test_bin )

install( TARGETS function_stream_lib task_scheduler_lib DESTINATION lib )
install( DIRECTORY ${HEADER_FOLDER}/ DESTINATION include/daw/fs )

