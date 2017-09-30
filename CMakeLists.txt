cmake_minimum_required( VERSION 2.8.12 )

project( function_stream_prj )

include( ExternalProject )

find_package( Boost 1.58.0 COMPONENTS system iostreams filesystem regex unit_test_framework REQUIRED )

enable_testing( )
add_definitions( -DBOOST_ALL_NO_LIB ) 

find_package( Threads REQUIRED )

if( ${CMAKE_CXX_COMPILER_ID} STREQUAL 'MSVC' )
	add_compile_options( -D_WIN32_WINNT=0x0601 /std:c++latest ) 
else( )
	if( ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang" )
		add_compile_options(-std=c++14 -march=native -pthread -Weverything -Wno-c++98-compat -Wno-covered-switch-default -Wno-padded -Wno-exit-time-destructors -Wno-c++98-compat-pedantic -Wno-unused-parameter -Wno-missing-noreturn -Wno-missing-prototypes -Wno-disabled-macro-expansion)
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
	elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" )
		add_compile_options(-std=c++14 -march=native -pthread -Wall -Wno-deprecated-declarations -Wduplicated-cond -Wlogical-op -Wnull-dereference -Wold-style-cast -Wshadow)
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
	endif( )
endif( )

externalproject_add(
    header_libraries_prj
    GIT_REPOSITORY "https://github.com/beached/header_libraries.git"
    SOURCE_DIR "${CMAKE_BINARY_DIR}/dependencies/header_libraries"
    GIT_TAG "master"
    INSTALL_DIR "${CMAKE_BINARY_DIR}/install"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/install
)

externalproject_add(
	date_prj
	GIT_REPOSITORY "https://github.com/beached/date.git"
	SOURCE_DIR "${CMAKE_BINARY_DIR}/dependencies/date"
	GIT_TAG "master"
	INSTALL_DIR "${CMAKE_BINARY_DIR}/install"
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/install
)


set( HEADER_FOLDER "include" )
set( SOURCE_FOLDER "src" )
set( TEST_FOLDER "tests" )

include_directories( SYSTEM "${CMAKE_BINARY_DIR}/install/include" )
include_directories( ${HEADER_FOLDER} )

include_directories( SYSTEM ${Boost_INCLUDE_DIRS} )

link_directories( "${CMAKE_BINARY_DIR}/install/lib" )
link_directories( ${Boost_LIBRARY_DIRS} )

set( FUNCTION_STREAM_HEADER_FILES
	${HEADER_FOLDER}/algorithms.h
	${HEADER_FOLDER}/algorithms_impl.h
	${HEADER_FOLDER}/function_stream.h
	${HEADER_FOLDER}/function_stream_impl.h
	${HEADER_FOLDER}/future_result.h
	${HEADER_FOLDER}/future_result_impl.h
	${HEADER_FOLDER}/iterator_range.h
	${HEADER_FOLDER}/package.h
	${HEADER_FOLDER}/message_queue.h
	${HEADER_FOLDER}/spin_lock.h
	${HEADER_FOLDER}/task_scheduler.h
	${HEADER_FOLDER}/task_scheduler_impl.h
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

add_library( task_scheduler_lib ${TASK_SCHEDULER_HEADER_FILES} ${TASK_SCHEDULER_SOURCE_FILES} )
target_link_libraries( task_scheduler_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_dependencies( task_scheduler_lib header_libraries_prj )

add_library( function_stream_lib ${FUNCTION_STREAM_HEADER_FILES} ${TASK_SCHEDULER_HEADER_FILES} ${FUNCTION_STREAM_SOURCE_FILES} )
target_link_libraries( function_stream_lib task_scheduler_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_dependencies( function_stream_lib header_libraries_prj )

add_executable( task_scheduler_test_bin ${TEST_FOLDER}/task_scheduler_test.cpp )
target_link_libraries( task_scheduler_test_bin task_scheduler_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_dependencies( task_scheduler_test_bin header_libraries_prj )
add_test( task_scheduler_test task_scheduler_test_bin )

add_executable( function_stream_test_bin ${FUNCTION_STREAM_HEADER_FILES} ${TASK_SCHEDULER_HEADER_FILES} ${TEST_FOLDER}/function_stream_test.cpp )
target_link_libraries( function_stream_test_bin task_scheduler_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )

add_executable( algorithms_test_bin ${TEST_FOLDER}/algorithms_test.cpp )
target_link_libraries( algorithms_test_bin task_scheduler_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_dependencies( algorithms_test_bin date_prj header_libraries_prj )
add_test( algorithms_test algorithms_test_bin )

add_executable( map_reduce_test_bin ${FUNCTION_STREAM_HEADER_FILES} ${TASK_SCHEDULER_HEADER_FILES} ${TEST_FOLDER}/map_reduce_test.cpp )
target_link_libraries( map_reduce_test_bin task_scheduler_lib function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_dependencies( map_reduce_test_bin date_prj header_libraries_prj )


install( TARGETS function_stream_lib task_scheduler_lib DESTINATION lib )
install( DIRECTORY ${HEADER_FOLDER}/ DESTINATION include/daw/fs )

