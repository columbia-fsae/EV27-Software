cmake_minimum_required(VERSION 3.22)

#
# Host-side unit tests for the Tractive Battery firmware (Unity + FFF).
#
# This is a separate CMake project from ../CMakeLists.txt on purpose: the
# firmware build uses the arm-none-eabi toolchain, while tests are compiled
# with the PC's own C compiler (gcc, clang or zig cc). From
# High_Voltage/Tractive_Battery:
#
#   cmake -S Tests -B Tests/build
#   cmake --build Tests/build
#   ctest --test-dir Tests/build --output-on-failure
#
# To add a test, see the "Tests" section at the bottom and Tests/TUTORIAL.md.
#

project(TractiveBatteryTests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

enable_testing()

set(FW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
cmake_path(NORMAL_PATH FW_DIR)
string(REGEX REPLACE "/$" "" FW_DIR "${FW_DIR}")

# Runner generation, and the TB_TESTS_SANITIZE / _WERROR / _COVERAGE options
include(cmake/runner.cmake)
include(cmake/quality.cmake OPTIONAL)

# ---------------------------------------------------------------- Libraries

# Unity test framework (vendor/unity, v2.7.0)
add_library(unity STATIC vendor/unity/unity.c)
target_include_directories(unity PUBLIC vendor/unity)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE UNITY_SUPPORT_64)

# FFF fakes for the STM32 HAL (vendor/fff, v1.1), linked into every test. The
# fakes/ directory also holds a stub stm32g4xx_hal.h that replaces the real HAL
# headers, and the fake_<module>.{h,c} libraries used through FAKES.
add_library(hal_fakes STATIC fakes/hal_fakes.c)
target_include_directories(hal_fakes PUBLIC fakes vendor/fff)

# Shared test helpers (support/), such as the ISR simulator for checking data
# shared between interrupts and the main loop / RTOS tasks.
find_package(Threads REQUIRED)
add_library(test_support STATIC support/isr_sim.c)
target_include_directories(test_support PUBLIC support)
target_link_libraries(test_support PUBLIC hal_fakes Threads::Threads)

# ---------------------------------------------------------------- Firmware paths

# Firmware headers shared by all tests. Add a folder here when a new firmware
# module's headers live somewhere else.
set(FW_INCLUDES
    ${FW_DIR}/ADBMS6830/program/inc
    ${FW_DIR}/ADBMS6830/lib/inc
    ${FW_DIR}/BSM/inc
    ${FW_DIR}/Communications/inc
)

# Folders add_fw_test searches for <module>.c. Add a folder here when new
# firmware sources live somewhere else.
set(FW_SRC_DIRS
    ${FW_DIR}/BSM/src
    ${FW_DIR}/Communications/src
    ${FW_DIR}/ADBMS6830/program/src
    ${FW_DIR}/ADBMS6830/lib/src
)

# ---------------------------------------------------------------- add_fw_test

# Finds <module>.c in FW_SRC_DIRS. Fails configure unless exactly one matches.
function(tb_find_fw_source module out_var context)
    set(found "")
    foreach(dir IN LISTS FW_SRC_DIRS)
        if(EXISTS "${dir}/${module}.c")
            list(APPEND found "${dir}/${module}.c")
        endif()
    endforeach()
    list(LENGTH found count)
    if(count EQUAL 0)
        list(JOIN FW_SRC_DIRS "\n    " dirs)
        message(FATAL_ERROR
            "${context}: no firmware source named ${module}.c. Looked in:\n    ${dirs}\n"
            "Check the spelling (it is case-sensitive), or pass NO_DEFAULT_SOURCE "
            "and list the files under SOURCES.")
    elseif(count GREATER 1)
        list(JOIN found "\n    " files)
        message(FATAL_ERROR
            "${context}: ${module}.c matches more than one file:\n    ${files}\n"
            "Pass NO_DEFAULT_SOURCE and list the one you mean under SOURCES.")
    endif()
    set(${out_var} "${found}" PARENT_SCOPE)
endfunction()

# add_fw_test(<module>
#     [SOURCES <real firmware modules or .c paths to link too>]
#     [FAKES   <modules to replace with fakes/fake_<module>.{h,c}>]
#     [DEFINES <macros>]
#     [NO_DEFAULT_SOURCE])
#
# Builds the test_<module> executable and ctest test from test_<module>.c, the
# real <module>.c and a generated runner (main() is written for you).
function(add_fw_test module)
    cmake_parse_arguments(ARG "NO_DEFAULT_SOURCE" "" "SOURCES;FAKES;DEFINES" ${ARGN})
    set(target test_${module})
    set(ctx "add_fw_test(${module})")

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "${ctx}: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    set(test_src ${CMAKE_CURRENT_SOURCE_DIR}/${target}.c)
    if(NOT EXISTS ${test_src})
        message(FATAL_ERROR "${ctx}: test file not found: ${test_src}")
    endif()

    set(fw_sources "")
    set(real_modules "")
    if(NOT ARG_NO_DEFAULT_SOURCE)
        tb_find_fw_source(${module} default_src "${ctx}")
        list(APPEND fw_sources ${default_src})
    endif()
    foreach(src IN LISTS ARG_SOURCES)
        if(NOT src MATCHES "[/\\\\]" AND NOT src MATCHES "\\.c$")
            tb_find_fw_source(${src} src "${ctx} SOURCES")
        elseif(NOT IS_ABSOLUTE ${src})
            set(src ${FW_DIR}/${src})
        endif()
        if(NOT EXISTS ${src})
            message(FATAL_ERROR "${ctx}: SOURCES file not found: ${src}")
        endif()
        list(APPEND fw_sources ${src})
    endforeach()
    foreach(src IN LISTS fw_sources)
        get_filename_component(name ${src} NAME_WE)
        list(APPEND real_modules ${name})
    endforeach()

    set(fake_sources "")
    foreach(fake IN LISTS ARG_FAKES)
        if(fake STREQUAL module)
            message(FATAL_ERROR "${ctx}: FAKES lists ${fake}, the module under test.")
        endif()
        if(fake IN_LIST real_modules)
            message(FATAL_ERROR
                "${ctx}: ${fake} is in both SOURCES and FAKES. Link the real module "
                "or its fake, not both.")
        endif()
        set(fake_src ${CMAKE_CURRENT_SOURCE_DIR}/fakes/fake_${fake}.c)
        if(NOT EXISTS ${fake_src})
            message(FATAL_ERROR
                "${ctx}: FAKES ${fake}: ${fake_src} does not exist. Write "
                "fakes/fake_${fake}.{h,c} (see fakes/fake_can_management.h for the pattern).")
        endif()
        list(APPEND fake_sources ${fake_src})
    endforeach()

    set(runner ${CMAKE_BINARY_DIR}/runners/${target}_runner.c)
    tb_generate_runner(${test_src} ${runner} FAKES ${ARG_FAKES})

    add_executable(${target} ${test_src} ${runner} ${fw_sources} ${fake_sources})
    target_include_directories(${target} PRIVATE ${FW_INCLUDES})
    target_compile_definitions(${target} PRIVATE ${ARG_DEFINES})
    target_compile_options(${target} PRIVATE -Wall -Wextra)
    target_link_libraries(${target} PRIVATE unity hal_fakes test_support m)
    if(COMMAND tb_apply_quality_flags)
        tb_apply_quality_flags(${target})
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ---------------------------------------------------------------- Build info

# How a firmware .c is compiled here, for tools/needs_fakes.py
string(STRIP "${CMAKE_C_COMPILER_ARG1}" cc_arg1)
string(STRIP "${CMAKE_C_COMPILER} ${cc_arg1}" fw_test_cc)
file(WRITE ${CMAKE_BINARY_DIR}/fw_test_cc.txt "${fw_test_cc}\n")
set(fw_test_flags "")
foreach(dir IN LISTS FW_INCLUDES ITEMS ${CMAKE_CURRENT_SOURCE_DIR}/fakes
                                       ${CMAKE_CURRENT_SOURCE_DIR}/vendor/fff)
    string(APPEND fw_test_flags "-I${dir}\n")
endforeach()
string(APPEND fw_test_flags "-std=gnu11\n")
file(WRITE ${CMAKE_BINARY_DIR}/fw_test_flags.txt "${fw_test_flags}")
list(JOIN FW_SRC_DIRS "\n" fw_src_dirs)
file(WRITE ${CMAKE_BINARY_DIR}/fw_src_dirs.txt "${fw_src_dirs}\n")

# ---------------------------------------------------------------- Tests
#
# Add one line per firmware module here, for Tests/test_<module>.c.
# `python Tests/tools/needs_fakes.py <module>` prints the line to use.
# New test functions in an existing file need no change here.

add_fw_test(adc_management)
add_fw_test(bsm)

# TODO: test_kalman_soc.c and test_soc_flash_storage.c have not been written
# yet. Uncomment each block once its test file exists.
# add_fw_test(kalman_soc SOURCES battery_model)

# STM32G474xx/STM32G4 turn on the HAL flash code paths that are normally
# compiled only for the target. Their flash HAL types and fakes don't exist
# yet; `needs_fakes.py soc_flash_storage -D STM32G474xx -D STM32G4` lists them.
# add_fw_test(soc_flash_storage
#     SOURCES kalman_soc battery_model
#     DEFINES STM32G474xx STM32G4
# )
