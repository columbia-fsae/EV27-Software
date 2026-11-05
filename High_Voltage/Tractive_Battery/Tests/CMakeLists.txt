cmake_minimum_required(VERSION 3.22)

#
# Host-side unit tests for the Tractive Battery firmware.
#
# This is a separate CMake project from ../CMakeLists.txt on purpose: the
# firmware build uses the arm-none-eabi toolchain, while tests are compiled
# with the PC's native gcc. Build and run with:
#
#   cmake -S Tests -B Tests/build
#   cmake --build Tests/build
#   ctest --test-dir Tests/build --output-on-failure
#

project(TractiveBatteryTests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

enable_testing()

set(FW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Unity test framework (vendor/unity, v2.7.0)
add_library(unity STATIC vendor/unity/unity.c)
target_include_directories(unity PUBLIC vendor/unity)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE UNITY_SUPPORT_64)

# FFF fakes for the STM32 HAL (vendor/fff, v1.1). The fakes/ directory also
# holds a stub stm32g4xx_hal.h that replaces the real HAL headers.
add_library(hal_fakes STATIC fakes/hal_fakes.c)
target_include_directories(hal_fakes PUBLIC fakes vendor/fff)

# Shared test helpers (support/), such as the ISR simulator for checking data
# shared between interrupts and the main loop / RTOS tasks.
find_package(Threads REQUIRED)
add_library(test_support STATIC support/isr_sim.c)
target_include_directories(test_support PUBLIC support)
target_link_libraries(test_support PUBLIC hal_fakes Threads::Threads)

# Firmware headers shared by all tests
set(FW_INCLUDES
    ${FW_DIR}/ADBMS6830/program/inc
    ${FW_DIR}/ADBMS6830/lib/inc
    ${FW_DIR}/BSM/inc
    ${FW_DIR}/Communications/inc
)

# add_fw_test(<name> SOURCES <firmware .c files> [DEFINES <macros>])
function(add_fw_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEFINES" ${ARGN})
    add_executable(${name} ${name}.c ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE ${FW_INCLUDES})
    target_compile_definitions(${name} PRIVATE ${ARG_DEFINES})
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    target_link_libraries(${name} PRIVATE unity hal_fakes test_support m)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_fw_test(test_adc_management
    SOURCES
        ${FW_DIR}/Communications/src/adc_management.c
)

# TODO: test_kalman_soc.c and test_soc_flash_storage.c have not been written
# yet. Uncomment each block once its test file exists.
# add_fw_test(test_kalman_soc
#     SOURCES
#         ${FW_DIR}/ADBMS6830/program/src/kalman_soc.c
#         ${FW_DIR}/ADBMS6830/program/src/battery_model.c
# )

# STM32G474xx/STM32G4 turn on the HAL code paths that are normally compiled
# only for the target, so the FFF fakes get called.
# add_fw_test(test_soc_flash_storage
#     SOURCES
#         ${FW_DIR}/ADBMS6830/program/src/soc_flash_storage.c
#         ${FW_DIR}/ADBMS6830/program/src/kalman_soc.c
#         ${FW_DIR}/ADBMS6830/program/src/battery_model.c
#     DEFINES
#         STM32G474xx
#         STM32G4
# )
