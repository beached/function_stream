cmake_minimum_required( VERSION 2.8.12 )

project( function_stream_prj )

include( ExternalProject )

find_package( Boost 1.60.0 REQUIRED )

enable_testing( )
add_definitions( -DBOOST_ALL_NO_LIB ) 

find_package( Threads REQUIRED )

if( ${CMAKE_CXX_COMPILER_ID} STREQUAL 'MSVC' )
	add_compile_options( -D_WIN32_WINNT=0x0601 /std:c++latest ) 
else( )
	if( ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" )
		add_compile_options(-std=c++14 -march=native -ggdb -Weverything -ftemplate-backtrace-limit=0  -pthread -Wno-c++98-compat -g -Wno-covered-switch-default -Wno-padded -Wno-exit-time-destructors -Wno-c++98-compat-pedantic -Wno-unused-parameter -Wno-missing-noreturn -Wno-missing-prototypes -Wno-disabled-macro-expansion)
	elseif( ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" )
		add_compile_options(-std=c++14 -march=native -ggdb -Wall -pthread -Wno-deprecated-declarations)
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

set( HEADER_FOLDER "include" )
set( SOURCE_FOLDER "src" )
set( TEST_FOLDER "tests" )

include_directories( SYSTEM "${CMAKE_BINARY_DIR}/install/include" )
include_directories( ${HEADER_FOLDER} )

include_directories( SYSTEM ${Boost_INCLUDE_DIRS} )

link_directories( "${CMAKE_BINARY_DIR}/install/lib" )
link_directories( ${Boost_LIBRARY_DIRS} )

set( HEADER_FILES
	${HEADER_FOLDER}/algorithms.h
	${HEADER_FOLDER}/function_stream.h
	${HEADER_FOLDER}/future_result.h
	${HEADER_FOLDER}/package.h
	${HEADER_FOLDER}/task_scheduler.h
)

set( SOURCE_FILES
	${SOURCE_FOLDER}/future_result.cpp
	${SOURCE_FOLDER}/task_scheduler.cpp
)

add_library( function_stream_lib ${HEADER_FILES} ${SOURCE_FILES} )
target_link_libraries( function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_dependencies( function_stream_lib header_libraries_prj )

add_executable( task_scheduler_test_bin ${TEST_FOLDER}/task_scheduler_test.cpp )
target_link_libraries( task_scheduler_test_bin function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )
add_dependencies( task_scheduler_test_bin header_libraries_prj )

add_executable( function_stream_test_bin ${HEADER_FILES} ${TEST_FOLDER}/function_stream_test.cpp )
target_link_libraries( function_stream_test_bin function_stream_lib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )

