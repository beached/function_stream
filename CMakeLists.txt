cmake_minimum_required(VERSION 3.13)

project("function_stream"
        VERSION "0.1.0"
        HOMEPAGE_URL "https://github.com/beached/function_stream"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "The C++ standard whose features are requested.")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
message("CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}")

add_subdirectory(extern)
option(DAW_ENABLE_TESTING "Build unit tests" OFF)

find_package(Threads REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if (NOT MSVC)
    find_package(TBB)
    if (TBB_LIBRARIES)
        message("Found TBB")
        add_definitions(${TBB_DEFINITIONS} -DHAS_PAR_STL)
        include_directories(SYSTEM ${TBB_INCLUDE_DIRS})
        link_libraries(${TBB_LIBRARIES})
    else ()
        message("TBB not found")
    endif ()
endif ()

include(GNUInstallDirs)
set(function_stream_INSTALL_CMAKEDIR
        "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake"
        CACHE STRING
        "The install directory to use for CMake package config files")

set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

set(HEADER_FOLDER "include")
set(SOURCE_FOLDER "src")
set(TEST_FOLDER "tests")

add_library(display_info)
add_library(daw::display_info ALIAS display_info)
target_link_libraries(display_info daw::header_libraries date::date)
target_compile_features(display_info INTERFACE cxx_std_17)
target_include_directories(display_info
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        )

target_sources(display_info
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/display_info.h
        PRIVATE
        ${SOURCE_FOLDER}/display_info.cpp
        )


add_library(task_scheduler)
add_library(daw::task_scheduler ALIAS task_scheduler)
target_link_libraries(task_scheduler daw::header_libraries ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(task_scheduler INTERFACE cxx_std_17)
target_include_directories(task_scheduler
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        )

target_sources(task_scheduler
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/task_scheduler.h
        PRIVATE
        ${SOURCE_FOLDER}/task_scheduler.cpp
        )

add_library(function_stream)
add_library(daw::function_stream ALIAS function_stream)
target_link_libraries(function_stream daw::header_libraries daw::task_scheduler ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(function_stream INTERFACE cxx_std_17)
target_include_directories(function_stream
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        )

target_sources(function_stream
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/algorithms.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/function_stream.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/future_result.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/package.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/message_queue.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/task_scheduler.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/algorithms_impl.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/concept_checks.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/dbg_proxy.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/ithread.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/function_stream_impl.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/future_result_impl.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/task.h
        PRIVATE
        ${SOURCE_FOLDER}/future_result.cpp
        )

#include(CMakePackageConfigHelpers)

#configure_package_config_file(
#        "${PROJECT_SOURCE_DIR}/cmake/daw_${PROJECT_NAME}Config.cmake.in"
#        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
#        INSTALL_DESTINATION ${function_stream_INSTALL_CMAKEDIR})

#write_basic_package_version_file("${PROJECT_NAME}ConfigVersion.cmake"
#        VERSION ${PROJECT_VERSION}
#        COMPATIBILITY SameMajorVersion)

#install(EXPORT ${PROJECT_NAME}_Targets
#        FILE ${PROJECT_NAME}Targets.cmake
#        NAMESPACE daw::
#        DESTINATION ${function_stream_INSTALL_CMAKEDIR})

#install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
#        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
#        DESTINATION ${function_stream_INSTALL_CMAKEDIR}$)

#install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)

option(DAW_ENABLE_TESTING "Build unit tests" OFF)
if (DAW_ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif ()


