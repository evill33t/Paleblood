# SPDX-License-Identifier: GPL-2.0-or-later
# Owned output stream checks use registered providers and real host descriptors.
add_executable(test_console ${PROJECT_SOURCE_DIR}/test/test_console.c
               ${PROJECT_SOURCE_DIR}/runtime/syslib/table.c)
target_include_directories(test_console PRIVATE ${PROJECT_SOURCE_DIR}/runtime/include)
target_link_libraries(test_console PRIVATE Threads::Threads)
foreach(console_case regular errors pipe timeout prepare-failure)
    string(REPLACE "-" "_" console_test "console_${console_case}")
    add_test(NAME ${console_test} COMMAND test_console ${console_case})
    set_tests_properties(${console_test} PROPERTIES TIMEOUT 15)
endforeach()
add_test(NAME console_boot
         COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/test/test_console_boot.py -v)
set_tests_properties(console_boot PROPERTIES TIMEOUT 45
    ENVIRONMENT "BB_PBBOOT=$<TARGET_FILE:pbboot>")
