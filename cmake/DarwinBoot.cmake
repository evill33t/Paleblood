# SPDX-License-Identifier: GPL-2.0-or-later
# Selected own-runtime C startup only; no game library, Linux probe, Metal bridge or GPU test.
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
add_subdirectory(runtime)
add_executable(test_boot_darwin test/test_boot_darwin.c ${PB_BOOT_SUPPORT_SOURCES})
target_include_directories(test_boot_darwin PRIVATE runtime/include)
target_link_libraries(test_boot_darwin PRIVATE pbdirect_memory Threads::Threads)
add_test(NAME darwin_boot_cleanup COMMAND test_boot_darwin)
set_tests_properties(darwin_boot_cleanup PROPERTIES TIMEOUT 15)
add_test(NAME darwin_owned_boot
         COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/test_boot_darwin.py -v)
set_tests_properties(darwin_owned_boot PROPERTIES TIMEOUT 45
    ENVIRONMENT "BB_PBBOOT=$<TARGET_FILE:pbboot>")
# Synthetic mapping ownership failure; exact helper included with test-only libc aliases.
add_executable(test_boot_darwin_zero_map test/test_boot_darwin_zero_map.c)
target_include_directories(test_boot_darwin_zero_map PRIVATE runtime/include)
add_test(NAME darwin_zero_map_ownership COMMAND test_boot_darwin_zero_map)
set_tests_properties(darwin_zero_map_ownership PROPERTIES TIMEOUT 10)
