# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.20)
project(paleblood C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # editors: ln -s build/compile_commands.json .
# Verification proves the code as it is built, so every build is an optimized one unless asked
# otherwise (CI builds Release too).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
add_compile_options(-Wall -Wextra -Werror)

enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
add_subdirectory(runtime)
add_subdirectory(game)

add_executable(test_hooks test/test_hooks.c)
target_link_libraries(test_hooks bbrt)
add_test(NAME hooks COMMAND test_hooks)
add_executable(test_plugin test/test_plugin.c runtime/plugin.c runtime/hooks.c)
target_include_directories(test_plugin PRIVATE runtime/include)
add_test(NAME plugin COMMAND test_plugin)

add_executable(test_host_hooks test/test_host_hooks.c runtime/host_hooks.c)
target_include_directories(test_host_hooks PRIVATE runtime/include)
add_test(NAME host_hooks COMMAND test_host_hooks)
add_executable(test_capture test/test_capture.cpp)
target_link_libraries(test_capture bbrt)
add_test(NAME capture COMMAND test_capture)
add_executable(test_capture_poc test/test_capture_poc.cpp)
add_test(NAME capture_poc_fail_closed COMMAND test_capture_poc --self-test)
add_test(NAME capture_poc_replay
         COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/capture_poc_integration.py $<TARGET_FILE:test_capture_poc>)
set_tests_properties(capture_poc_replay PROPERTIES
    ENVIRONMENT "CC=${CMAKE_C_COMPILER};BB_LOADER=$<TARGET_FILE:pbloader>")
# System library implementations, compiled in directly so the linker keeps every RT_SYSLIB entry
file(GLOB PB_TEST_SYSLIB CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/runtime/syslib/*.c)
add_executable(test_syslib test/test_syslib.c runtime/process.c runtime/cpu.c ${PB_TEST_SYSLIB})
target_include_directories(test_syslib PRIVATE runtime/include)
target_link_libraries(test_syslib PRIVATE pbdirect_memory Threads::Threads)
add_test(NAME syslib COMMAND test_syslib)
add_executable(test_affinity test/test_affinity.c runtime/cpu.c runtime/process.c
               runtime/syslib/kernel_thread.c runtime/syslib/table.c)
target_include_directories(test_affinity PRIVATE runtime/include)
add_test(NAME affinity COMMAND test_affinity)

add_executable(test_direct_memory test/test_direct_memory.c)
target_link_libraries(test_direct_memory PRIVATE pbdirect_memory)
add_test(NAME direct_memory COMMAND test_direct_memory)
add_executable(test_direct_memory_registry test/test_direct_memory_registry.c runtime/process.c
               runtime/syslib/kernel_memory.c runtime/syslib/table.c)
target_link_libraries(test_direct_memory_registry PRIVATE pbdirect_memory Threads::Threads)
add_test(NAME direct_memory_registry COMMAND test_direct_memory_registry)
# Exercise the exact production core's rollback paths through test-only libc aliases.
add_library(direct_memory_fault_core OBJECT runtime/direct_memory.c)
target_include_directories(direct_memory_fault_core PRIVATE runtime/include)
target_compile_definitions(direct_memory_fault_core PRIVATE mmap=rt_test_mmap munmap=rt_test_munmap
                           calloc=rt_test_calloc tmpfile=rt_test_tmpfile ftruncate=rt_test_ftruncate
                           fclose=rt_test_fclose)
add_executable(test_direct_memory_faults test/test_direct_memory_faults.c
               $<TARGET_OBJECTS:direct_memory_fault_core>)
target_include_directories(test_direct_memory_faults PRIVATE runtime/include)
add_test(NAME direct_memory_faults COMMAND test_direct_memory_faults)
set_tests_properties(direct_memory direct_memory_registry direct_memory_faults PROPERTIES TIMEOUT 30)
# Exercise a pending boot timeout inside the exact core's allocation-ledger call.
add_library(direct_memory_timeout_core OBJECT runtime/direct_memory.c)
target_include_directories(direct_memory_timeout_core PRIVATE runtime/include)
target_compile_definitions(direct_memory_timeout_core PRIVATE calloc=rt_timeout_calloc free=rt_timeout_free)
add_executable(test_direct_memory_timeout test/test_direct_memory_timeout.c runtime/process.c
               runtime/syslib/kernel_memory.c runtime/syslib/table.c
               $<TARGET_OBJECTS:direct_memory_timeout_core>)
target_include_directories(test_direct_memory_timeout PRIVATE runtime/include)
target_link_libraries(test_direct_memory_timeout PRIVATE Threads::Threads)
add_test(NAME direct_memory_timeout COMMAND test_direct_memory_timeout)
set_tests_properties(direct_memory_timeout PROPERTIES TIMEOUT 10)


add_test(NAME tools COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${PROJECT_SOURCE_DIR}/test -p "test_*.py" -v)
set_tests_properties(tools PROPERTIES ENVIRONMENT "BB_LOADER=$<TARGET_FILE:pbloader>;BB_PBBOOT=$<TARGET_FILE:pbboot>")
add_test(NAME verify_self_test COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/verify.py --self-test)
add_test(NAME validate_functions COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/validate_functions.py)
add_test(NAME check_agnostic COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_agnostic.py)
add_test(NAME float_flags COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_float_flags.py ${CMAKE_BINARY_DIR})
add_test(NAME progress_readme COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/progress.py --check)
