# Copyright (c) Darrell Wright
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/beached/function_stream
#

include(cmake/test_compiler_options.cmake)

add_custom_target(full)

add_executable(task_scheduler_test_bin EXCLUDE_FROM_ALL src/task_scheduler_test.cpp)
target_link_libraries(task_scheduler_test_bin daw::task_scheduler ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(task_scheduler_test_bin PRIVATE include)
add_test(task_scheduler_test task_scheduler_test_bin)
add_dependencies(full task_scheduler_test_bin)

add_executable(message_queue_test_bin EXCLUDE_FROM_ALL src/message_queue_test.cpp)
target_link_libraries(message_queue_test_bin daw::daw-header-libraries ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(message_queue_test_bin PRIVATE include)
add_test(message_queue_test message_queue_test_bin)
add_dependencies(full message_queue_test_bin)

add_executable(function_stream_test_bin EXCLUDE_FROM_ALL src/function_stream_test.cpp)
target_link_libraries(function_stream_test_bin daw::task_scheduler daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(function_stream_test_bin PRIVATE include)
add_test(function_stream_test function_stream_test_bin)
add_dependencies(full function_stream_test_bin)

add_executable(future_result_test_bin EXCLUDE_FROM_ALL src/future_result_test.cpp)
target_link_libraries(future_result_test_bin daw::task_scheduler daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(future_result_test_bin PRIVATE include)
add_test(future_result_test future_result_test_bin)
add_dependencies(full future_result_test_bin)

add_executable(function_composition_test_bin EXCLUDE_FROM_ALL src/function_composition_test.cpp)
target_link_libraries(function_composition_test_bin daw::task_scheduler daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(function_composition_test_bin PRIVATE include)
add_test(function_composition_test function_composition_test_bin)
add_dependencies(full function_composition_test_bin)

add_executable(function_composition_test_bin2 EXCLUDE_FROM_ALL src/function_composition_test2.cpp)
target_link_libraries(function_composition_test_bin2 daw::task_scheduler daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(function_composition_test_bin2 PRIVATE include)
add_test(function_composition_test2 function_composition_test_bin2)
add_dependencies(full function_composition_test_bin2)

add_executable(algorithms_for_each_test_bin EXCLUDE_FROM_ALL src/algorithms_for_each_test.cpp)
target_link_libraries(algorithms_for_each_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_for_each_test_bin PRIVATE include)
add_test(algorithms_for_each_test algorithms_for_each_test_bin)
add_dependencies(full algorithms_for_each_test_bin)

add_executable(algorithms_fill_test_bin EXCLUDE_FROM_ALL src/algorithms_fill_test.cpp)
target_link_libraries(algorithms_fill_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_fill_test_bin PRIVATE include)
add_test(algorithms_fill_test algorithms_fill_test_bin)
add_dependencies(full algorithms_fill_test_bin)

add_executable(algorithms_find_if_test_bin EXCLUDE_FROM_ALL src/algorithms_find_if_test.cpp)
if (MSVC)
    target_link_libraries(algorithms_find_if_test_bin daw::task_scheduler daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
else ()
    #target_link_libraries( algorithms_find_if_test_bin daw::task_scheduler daw::function_stream ${CMAKE_THREAD_LIBS_INIT} )
    target_link_libraries(algorithms_find_if_test_bin daw::task_scheduler daw::function_stream ${TBB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif ()
target_include_directories(algorithms_find_if_test_bin PRIVATE include)
add_test(algorithms_find_if_test algorithms_find_if_test_bin)
add_dependencies(full algorithms_find_if_test_bin)

add_executable(algorithms_sort_test_bin EXCLUDE_FROM_ALL src/algorithms_sort_test.cpp)
if (MSVC)
    target_link_libraries(algorithms_sort_test_bin daw::task_scheduler daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
else ()
    #target_link_libraries( algorithms_sort_test_bin daw::task_scheduler daw::function_stream ${CMAKE_THREAD_LIBS_INIT} )
    target_link_libraries(algorithms_sort_test_bin daw::task_scheduler daw::function_stream ${TBB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif ()
target_include_directories(algorithms_sort_test_bin PRIVATE include)
add_test(algorithms_sort_test algorithms_sort_test_bin)
add_dependencies(full algorithms_sort_test_bin)

add_executable(algorithms_stable_sort_test_bin EXCLUDE_FROM_ALL src/algorithms_stable_sort_test.cpp)
if (MSVC)
    target_link_libraries(algorithms_stable_sort_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
else ()
    #target_link_libraries( algorithms_stable_sort_test_bin daw::task_scheduler daw::display_info daw::function_stream  ${CMAKE_THREAD_LIBS_INIT} )
    target_link_libraries(algorithms_stable_sort_test_bin daw::task_scheduler daw::display_info daw::function_stream ${TBB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif ()
target_include_directories(algorithms_stable_sort_test_bin PRIVATE include)

add_test(algorithms_stable_sort_test algorithms_stable_sort_test_bin)
add_dependencies(full algorithms_stable_sort_test_bin)

add_executable(algorithms_sort_par_test_bin EXCLUDE_FROM_ALL src/algorithms_sort_par_test.cpp)
target_link_libraries(algorithms_sort_par_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_sort_par_test_bin PRIVATE include)
add_test(algorithms_sort_par_test algorithms_sort_par_test_bin)
add_dependencies(full algorithms_sort_par_test_bin)

add_executable(algorithms_sort_seq_test_bin EXCLUDE_FROM_ALL src/algorithms_sort_seq_test.cpp)
target_link_libraries(algorithms_sort_seq_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_sort_seq_test_bin PRIVATE include)
add_test(algorithms_sort_seq_test algorithms_sort_seq_test_bin)
add_dependencies(full algorithms_sort_seq_test_bin)

add_executable(algorithms_reduce_test_bin EXCLUDE_FROM_ALL src/algorithms_reduce_test.cpp)
target_link_libraries(algorithms_reduce_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_reduce_test_bin PRIVATE include)
add_test(algorithms_reduce_test algorithms_reduce_test_bin)
add_dependencies(full algorithms_reduce_test_bin)

add_executable(algorithms_min_max_element_test_bin EXCLUDE_FROM_ALL src/algorithms_min_max_element_test.cpp)
target_link_libraries(algorithms_min_max_element_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_min_max_element_test_bin PRIVATE include)
add_test(algorithms_min_max_element_test algorithms_min_max_element_test_bin)
add_dependencies(full algorithms_min_max_element_test_bin)

add_executable(algorithms_transform_test_bin EXCLUDE_FROM_ALL src/algorithms_transform_test.cpp)
target_link_libraries(algorithms_transform_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_transform_test_bin PRIVATE include)
add_test(algorithms_transform_test algorithms_transform_test_bin)
add_dependencies(full algorithms_transform_test_bin)

add_executable(algorithms_map_reduce_test_bin EXCLUDE_FROM_ALL src/algorithms_map_reduce_test.cpp)
target_link_libraries(algorithms_map_reduce_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_map_reduce_test_bin PRIVATE include)
add_test(algorithms_map_reduce_test algorithms_map_reduce_test_bin)
add_dependencies(full algorithms_map_reduce_test_bin)

add_executable(algorithms_scan_test_bin EXCLUDE_FROM_ALL src/algorithms_scan_test.cpp)
target_link_libraries(algorithms_scan_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_scan_test_bin PRIVATE include)
add_test(algorithms_scan_test algorithms_scan_test_bin)
add_dependencies(full algorithms_scan_test_bin)

add_executable(algorithms_equal_test_bin EXCLUDE_FROM_ALL src/algorithms_equal_test.cpp)
target_link_libraries(algorithms_equal_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_equal_test_bin PRIVATE include)
add_test(algorithms_equal_test algorithms_equal_test_bin)
add_dependencies(full algorithms_equal_test_bin)

add_executable(algorithms_count_test_bin EXCLUDE_FROM_ALL src/algorithms_count_test.cpp)
target_link_libraries(algorithms_count_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_count_test_bin PRIVATE include)
add_test(algorithms_count_test algorithms_count_test_bin)
add_dependencies(full algorithms_count_test_bin)

add_executable(algorithms_chunked_for_each_test_bin EXCLUDE_FROM_ALL src/algorithms_chunked_for_each_test.cpp)
target_link_libraries(algorithms_chunked_for_each_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(algorithms_chunked_for_each_test_bin PRIVATE include)
add_test(algorithms_chunked_for_each_test algorithms_chunked_for_each_test_bin)
add_dependencies(full algorithms_chunked_for_each_test_bin)

add_executable(map_reduce_test_bin EXCLUDE_FROM_ALL src/map_reduce_test.cpp)
target_link_libraries(map_reduce_test_bin daw::task_scheduler daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(map_reduce_test_bin PRIVATE include)
add_test(map_reduce_test map_reduce_test_bin)
#add_dependencies( is_full map_reduce_test_bin )

add_executable(k-means_test_bin EXCLUDE_FROM_ALL src/k-means.cpp)
target_link_libraries(k-means_test_bin daw::task_scheduler daw::display_info daw::function_stream ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(k-means_test_bin PRIVATE include)
add_test(k-means_test k-means_test_bin)
add_dependencies(full k-means_test_bin)


