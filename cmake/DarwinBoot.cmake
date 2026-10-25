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
# Native-allowed mock providers and explicit logical slots remain separate policy domains.
add_executable(test_affinity test/test_affinity.c runtime/cpu.c runtime/logical_cpu.c runtime/process.c
               runtime/syslib/kernel_thread.c runtime/syslib/table.c)
target_include_directories(test_affinity PRIVATE runtime/include)
add_test(NAME affinity COMMAND test_affinity)
add_executable(test_logical_cpu_registry test/test_logical_cpu_registry.c runtime/cpu.c
               runtime/logical_cpu.c runtime/process.c runtime/syslib/kernel_thread.c runtime/syslib/table.c)
target_include_directories(test_logical_cpu_registry PRIVATE runtime/include)
add_test(NAME logical_cpu_registry COMMAND test_logical_cpu_registry)
set_tests_properties(affinity logical_cpu_registry PROPERTIES TIMEOUT 15)
add_executable(test_boot_logical_cpu test/test_boot_logical_cpu.c ${PB_BOOT_SUPPORT_SOURCES})
target_include_directories(test_boot_logical_cpu PRIVATE runtime/include)
target_link_libraries(test_boot_logical_cpu PRIVATE pbdirect_memory Threads::Threads)
add_test(NAME darwin_logical_cpu_boot
         COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/test_boot_logical_cpu.py -v)
set_tests_properties(darwin_logical_cpu_boot PROPERTIES TIMEOUT 60
    ENVIRONMENT "BB_PBBOOT=$<TARGET_FILE:pbboot>;BB_PBBOOT_LOGICAL_QUERY_TEST=$<TARGET_FILE:test_boot_logical_cpu>")

# Narrow rwlock lifecycle tests; CHECKs remain active in optimized builds.
add_executable(test_rwlock test/test_rwlock.c runtime/process.c
               runtime/syslib/table.c runtime/syslib/kernel_rwlock.c)
target_include_directories(test_rwlock PRIVATE runtime/include)
target_link_libraries(test_rwlock PRIVATE Threads::Threads)
add_test(NAME rwlock_lifecycle COMMAND test_rwlock)
add_executable(test_rwlock_failures test/test_rwlock_failures.c)
target_include_directories(test_rwlock_failures PRIVATE runtime/include)
target_link_libraries(test_rwlock_failures PRIVATE Threads::Threads)
add_test(NAME rwlock_failures COMMAND test_rwlock_failures)
set_tests_properties(rwlock_lifecycle rwlock_failures PROPERTIES TIMEOUT 10)

# Narrow semaphore lifecycle policy tests; checks remain active in Release.
add_executable(test_semaphore test/test_semaphore.c runtime/process.c
               runtime/syslib/table.c runtime/syslib/kernel_semaphore.c)
target_include_directories(test_semaphore PRIVATE runtime/include)
target_link_libraries(test_semaphore PRIVATE Threads::Threads)
add_test(NAME semaphore_lifecycle COMMAND test_semaphore)
add_executable(test_semaphore_failures test/test_semaphore_failures.c)
target_include_directories(test_semaphore_failures PRIVATE runtime/include)
target_link_libraries(test_semaphore_failures PRIVATE Threads::Threads)
add_test(NAME semaphore_failures COMMAND test_semaphore_failures)
set_tests_properties(semaphore_lifecycle semaphore_failures PROPERTIES TIMEOUT 10)

# Realtime wire conversion and failure-before-publication checks.
add_executable(test_timeval test/test_timeval.c runtime/process.c
               runtime/syslib/table.c runtime/syslib/kernel_time.c)
target_include_directories(test_timeval PRIVATE runtime/include)
add_test(NAME timeval COMMAND test_timeval)
add_executable(test_timeval_failures test/test_timeval_failures.c)
target_include_directories(test_timeval_failures PRIVATE runtime/include)
add_test(NAME timeval_failures COMMAND test_timeval_failures)
set_tests_properties(timeval timeval_failures PROPERTIES TIMEOUT 10)

# Compiled-SDK wire validation over the loader-owned process parameters.
add_executable(test_compiled_sdk test/test_compiled_sdk.c
               runtime/syslib/table.c runtime/syslib/kernel_sdk.c)
target_include_directories(test_compiled_sdk PRIVATE runtime/include)
add_test(NAME compiled_sdk COMMAND test_compiled_sdk)
set_tests_properties(compiled_sdk PROPERTIES TIMEOUT 10)

# Full-width module arguments and proven early invalid-ID statuses.
add_executable(test_sysmodule_wire test/test_sysmodule_wire.c
               runtime/syslib/table.c runtime/syslib/kernel_sysmodule.c)
target_include_directories(test_sysmodule_wire PRIVATE runtime/include)
add_test(NAME sysmodule_wire COMMAND test_sysmodule_wire)
set_tests_properties(sysmodule_wire PROPERTIES TIMEOUT 10)

# Owned per-thread module blocks and the actual TLS registry entry.
find_package(Threads REQUIRED)
add_executable(test_module_tls test/test_module_tls.c runtime/syslib/kernel_tls.c
               runtime/syslib/table.c)
target_include_directories(test_module_tls PRIVATE runtime/include)
target_link_libraries(test_module_tls PRIVATE Threads::Threads)
add_test(NAME module_tls COMMAND test_module_tls)
set_tests_properties(module_tls PROPERTIES TIMEOUT 10)

add_test(NAME module_tls_boot
         COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/test_module_tls_boot.py -v)
set_tests_properties(module_tls_boot PROPERTIES TIMEOUT 30
    ENVIRONMENT "BB_PBBOOT=$<TARGET_FILE:pbboot>")

add_executable(test_module test/test_module.c runtime/module.c
               runtime/syslib/kernel_module.c runtime/syslib/table.c)
target_include_directories(test_module PRIVATE runtime/include)
target_link_libraries(test_module PRIVATE Threads::Threads)
add_test(NAME module_start COMMAND test_module)
set_tests_properties(module_start PROPERTIES TIMEOUT 10)
add_test(NAME module_start_boot
         COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/test_module_boot.py -v)
set_tests_properties(module_start_boot PROPERTIES TIMEOUT 45
    ENVIRONMENT "BB_PBBOOT=$<TARGET_FILE:pbboot>")

# Explicit owned process data and its publication boundary (no firmware fixtures).
add_executable(test_process_data test/test_process_data.c runtime/syslib/table.c)
target_include_directories(test_process_data PRIVATE runtime/include)
target_link_libraries(test_process_data PRIVATE Threads::Threads)
add_test(NAME process_data COMMAND test_process_data)
add_test(NAME process_data_empty COMMAND test_process_data empty)
add_test(NAME process_data_trailing COMMAND test_process_data trailing)
add_test(NAME process_data_backslash COMMAND test_process_data backslash)
set_tests_properties(process_data process_data_empty process_data_trailing process_data_backslash
    PROPERTIES TIMEOUT 10)
add_test(NAME process_data_boot
         COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/test_process_data_boot.py -v)
set_tests_properties(process_data_boot PROPERTIES TIMEOUT 30
    ENVIRONMENT "BB_PBBOOT=$<TARGET_FILE:pbboot>")

# Metadata loader fixtures use a native host library in the Mac validation gate;
# this target runs the actual whole-process x86-64 guest fixture.
add_test(NAME weak_boot COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/test_weak_boot.py -v)
set_tests_properties(weak_boot PROPERTIES TIMEOUT 45 ENVIRONMENT "BB_PBBOOT=$<TARGET_FILE:pbboot>")
