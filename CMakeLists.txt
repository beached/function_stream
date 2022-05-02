cmake_minimum_required(VERSION 3.20)

project("function_stream"
        VERSION "0.1.1"
        HOMEPAGE_URL "https://github.com/beached/function_stream"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard whose features are requested.")
option(DAW_ENABLE_TESTING "Build unit tests" OFF)

if (NOT DAW_USE_PACKAGE_MANAGEMENT)
    add_subdirectory(extern)
else ()
    find_package(daw-header-libraries REQUIRED)
endif ()

set(Boost_NO_WARN_NEW_VERSIONS ON)
find_package(Boost 1.79 REQUIRED)
find_package(date CONFIG REQUIRED)
find_package(Threads REQUIRED)


include(GNUInstallDirs)
set(function_stream_INSTALL_CMAKEDIR
        "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake"
        CACHE STRING
        "The install directory to use for CMake package config files")

set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

set(HEADER_FOLDER "include")
set(SOURCE_FOLDER "src")
set(TEST_FOLDER "tests")

add_library(daw-display-info)
add_library(daw::daw-display-info ALIAS daw-display-info)
target_link_libraries(daw-display-info PUBLIC daw::daw-header-libraries date::date)
target_compile_features(daw-display-info INTERFACE cxx_std_20)
target_include_directories(daw-display-info
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        )

target_sources(daw-display-info
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/display_info.h
        PRIVATE
        ${SOURCE_FOLDER}/display_info.cpp
        )

add_library(daw-task-scheduler)
add_library(daw::daw-task-scheduler ALIAS daw-task-scheduler)
target_link_libraries(daw-task-scheduler PUBLIC Boost::boost daw::daw-header-libraries daw::daw-parallel ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(daw-task-scheduler INTERFACE cxx_std_20)
target_include_directories(daw-task-scheduler
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        )

target_sources(daw-task-scheduler
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/task_scheduler.h
        PRIVATE
        ${SOURCE_FOLDER}/task_scheduler.cpp
        )

add_library(daw-function-stream)
add_library(daw::daw-function-stream ALIAS daw-function-stream)
target_link_libraries(daw-function-stream PUBLIC
        ${CMAKE_THREAD_LIBS_INIT}
        Boost::boost
        daw::daw-header-libraries
        daw::daw-parallel
        )
target_compile_features(daw-function-stream INTERFACE cxx_std_20)
target_include_directories(daw-function-stream
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        )

target_sources(daw-function-stream
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/algorithms.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/function_stream.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/future_result.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/algorithms_impl.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/concept_checks.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/dbg_proxy.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/function_stream_impl.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/future_result_impl.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/impl/task.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/message_queue.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/package.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/daw/fs/task_scheduler.h
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


