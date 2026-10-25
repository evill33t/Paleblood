# SPDX-License-Identifier: GPL-2.0-or-later
# Process-wide identity, atomic publication, and inherited-profile rejection.
add_executable(test_process_identity ${PROJECT_SOURCE_DIR}/test/test_process_identity.c
               ${PROJECT_SOURCE_DIR}/runtime/syslib/kernel_identity.c
               ${PROJECT_SOURCE_DIR}/runtime/syslib/table.c)
target_include_directories(test_process_identity PRIVATE ${PROJECT_SOURCE_DIR}/runtime/include)
target_link_libraries(test_process_identity PRIVATE Threads::Threads)
foreach(identity_case regular failure fork)
    add_test(NAME process_identity_${identity_case} COMMAND test_process_identity ${identity_case})
    set_tests_properties(process_identity_${identity_case} PROPERTIES TIMEOUT 15)
endforeach()
add_test(NAME process_identity_boot
         COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/test_process_identity_boot.py -v)
set_tests_properties(process_identity_boot PROPERTIES TIMEOUT 45
    ENVIRONMENT "BB_PBBOOT=$<TARGET_FILE:pbboot>")
