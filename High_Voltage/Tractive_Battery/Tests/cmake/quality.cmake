# Build-quality options for the host unit tests.
#
#   TB_TESTS_SANITIZE  AddressSanitizer + UndefinedBehaviorSanitizer. Default ON
#                      when the compiler supports them (never on Windows).
#   TB_TESTS_WERROR    -Werror on test, fake and support code. Firmware sources
#                      keep their warnings as warnings. Default ON.
#   TB_TESTS_COVERAGE  --coverage, plus a `coverage` target that runs the tests
#                      and writes an HTML report to <build>/coverage/. Needs
#                      gcovr (pip install gcovr). Default OFF.
#
# CMakeLists.txt calls tb_apply_quality_flags(<target>) for every test.
#
# This file also runs as a script (cmake -P) for the coverage target, see the
# bottom of the file.

if(CMAKE_SCRIPT_MODE_FILE)
    # ------------------------------------------------ coverage script mode
    # Inputs: TB_BUILD_DIR, TB_FW_DIR, TB_CTEST, TB_GCOVR, TB_GCOV_EXECUTABLE
    file(GLOB_RECURSE gcda_files "${TB_BUILD_DIR}/*.gcda")
    if(gcda_files)
        file(REMOVE ${gcda_files})  # zero the counters from earlier runs
    endif()

    execute_process(
        COMMAND ${TB_CTEST} --test-dir ${TB_BUILD_DIR} --output-on-failure
        RESULT_VARIABLE ctest_result)

    set(out_dir ${TB_BUILD_DIR}/coverage)
    file(REMOVE_RECURSE ${out_dir})
    file(MAKE_DIRECTORY ${out_dir})
    set(gcov_args "")
    if(TB_GCOV_EXECUTABLE)
        set(gcov_args --gcov-executable "${TB_GCOV_EXECUTABLE}")
    endif()
    # Paths in the filters are relative to the working directory (Tractive_Battery/)
    execute_process(
        COMMAND ${TB_GCOVR} ${TB_BUILD_DIR}
                --root ${TB_FW_DIR}
                --filter "(BSM|Communications|ADBMS6830)/"
                --exclude "Tests/"
                ${gcov_args}
                --html-details ${out_dir}/index.html
                --print-summary
        WORKING_DIRECTORY ${TB_FW_DIR}
        RESULT_VARIABLE gcovr_result)
    if(NOT gcovr_result EQUAL 0)
        message(FATAL_ERROR "gcovr failed (${gcovr_result})")
    endif()
    message(STATUS "Coverage report: ${out_dir}/index.html")
    # The report is still useful when tests fail, so don't fail the target.
    # The test job is what reports failures.
    if(NOT ctest_result EQUAL 0)
        message(WARNING "Some tests failed; the coverage report covers the run anyway.")
    endif()
    return()
endif()

include_guard(GLOBAL)
include(CheckCSourceCompiles)
include(CMakePushCheckState)

set(TB_SANITIZE_FLAGS -fsanitize=address,undefined -fno-sanitize-recover=undefined
                      -fno-omit-frame-pointer)

# Sanitizers default to ON only where they work
set(tb_sanitize_default OFF)
if(NOT WIN32)
    cmake_push_check_state(RESET)
    list(JOIN TB_SANITIZE_FLAGS " " CMAKE_REQUIRED_FLAGS)
    set(CMAKE_REQUIRED_LINK_OPTIONS -fsanitize=address,undefined)
    set(CMAKE_REQUIRED_QUIET ON)
    check_c_source_compiles("int main(void) { return 0; }" TB_COMPILER_HAS_ASAN)
    cmake_pop_check_state()
    if(TB_COMPILER_HAS_ASAN)
        set(tb_sanitize_default ON)
    endif()
endif()

option(TB_TESTS_SANITIZE "Build tests with ASan + UBSan" ${tb_sanitize_default})
option(TB_TESTS_WERROR "Treat warnings in test, fake and support code as errors" ON)
option(TB_TESTS_COVERAGE "Build with --coverage and add a 'coverage' target" OFF)
set(TB_GCOV_EXECUTABLE "" CACHE STRING
    "gcov for gcovr to use, e.g. 'llvm-cov gcov' for clang. Empty: gcovr's default")
message(STATUS "TB_TESTS_SANITIZE=${TB_TESTS_SANITIZE} TB_TESTS_WERROR=${TB_TESTS_WERROR} "
               "TB_TESTS_COVERAGE=${TB_TESTS_COVERAGE}")

# Tests/ itself. Sources under it (or generated into the build dir) are test
# code; everything else is firmware.
get_filename_component(TB_TESTS_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# -Werror for the shared test libraries, once
function(tb_quality_werror_libs)
    get_property(done GLOBAL PROPERTY TB_QUALITY_LIBS_DONE)
    if(done OR NOT TB_TESTS_WERROR)
        return()
    endif()
    foreach(lib hal_fakes test_support)  # not unity: vendor code
        if(TARGET ${lib})
            target_compile_options(${lib} PRIVATE -Werror)
        endif()
    endforeach()
    set_property(GLOBAL PROPERTY TB_QUALITY_LIBS_DONE TRUE)
endfunction()

function(tb_apply_quality_flags target)
    if(TB_TESTS_SANITIZE)
        target_compile_options(${target} PRIVATE ${TB_SANITIZE_FLAGS})
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()

    if(TB_TESTS_COVERAGE)
        target_compile_options(${target} PRIVATE --coverage -O0)
        target_link_options(${target} PRIVATE --coverage)
        add_dependencies(coverage ${target})  # build tests before running them
    endif()

    if(TB_TESTS_WERROR)
        tb_quality_werror_libs()
        # Per source file, so firmware sources in the same target are left alone
        get_target_property(sources ${target} SOURCES)
        get_target_property(src_dir ${target} SOURCE_DIR)
        foreach(src IN LISTS sources)
            get_filename_component(path "${src}" ABSOLUTE BASE_DIR "${src_dir}")
            string(FIND "${path}" "${TB_TESTS_DIR}/" in_tests)
            string(FIND "${path}" "${CMAKE_BINARY_DIR}/" in_build)
            if(in_tests EQUAL 0 OR in_build EQUAL 0)
                get_property(opts SOURCE "${path}" PROPERTY COMPILE_OPTIONS)
                if(NOT "-Werror" IN_LIST opts)
                    set_property(SOURCE "${path}" APPEND PROPERTY COMPILE_OPTIONS -Werror)
                endif()
            endif()
        endforeach()
    endif()
endfunction()

if(TB_TESTS_COVERAGE)
    find_program(TB_GCOVR gcovr)
    if(NOT TB_GCOVR)
        message(WARNING "TB_TESTS_COVERAGE is ON but gcovr was not found (pip install gcovr). "
                        "The 'coverage' target will fail.")
        set(TB_GCOVR gcovr)
    endif()
    get_filename_component(tb_fw_dir "${TB_TESTS_DIR}/.." ABSOLUTE)
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND}
                -DTB_BUILD_DIR=${CMAKE_BINARY_DIR}
                -DTB_FW_DIR=${tb_fw_dir}
                -DTB_CTEST=${CMAKE_CTEST_COMMAND}
                -DTB_GCOVR=${TB_GCOVR}
                "-DTB_GCOV_EXECUTABLE=${TB_GCOV_EXECUTABLE}"
                -P ${CMAKE_CURRENT_LIST_FILE}
        COMMENT "Running tests and writing the coverage report"
        USES_TERMINAL)
endif()
