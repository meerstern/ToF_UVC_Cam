# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)
set(CMAKE_BUILD_TYPE "Debug")
set(OPTIMIZATION_FLAG 1)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
#set(FAMILY "rp2040")
#set(PICO_TINYUSB_NO_EXAMPLE_WARNINGS 1)
#set(CFG_TUSB_MCU OPT_MCU_RP2040)


# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/mond/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
#include(${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/family_support.cmake)
include(pico_sdk_import.cmake)

#family_get_project_name(UVC_RP2040 ${CMAKE_CURRENT_LIST_DIR})


project(UVC_RP2040 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(UVC_RP2040 UVC_RP2040.c)

target_include_directories(UVC_RP2040 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(UVC_RP2040 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/UVC_RP2040.c
    ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ugui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tmf8829_image.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tmf8829.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tmf8829_shim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tmf8829_app.c
)
#target_compile_definitions(UVC_RP2040 PRIVATE CFG_TUSB_MCU=OPT_MCU_RP2040)

pico_set_program_name(UVC_RP2040 "UVC_RP2040")
pico_set_program_version(UVC_RP2040 "0.1")

pico_enable_stdio_uart(UVC_RP2040 1)
pico_enable_stdio_usb(UVC_RP2040 0)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  PICO_DEFAULT_UART=1
  PICO_DEFAULT_UART_TX_PIN=8
  PICO_DEFAULT_UART_RX_PIN=9
)

# Add the standard library to the build
target_link_libraries(UVC_RP2040 pico_stdlib pico_rand tinyusb_device tinyusb_board pico_multicore)

target_link_libraries(UVC_RP2040
        hardware_spi
        hardware_i2c
        hardware_timer
        hardware_watchdog
        )

target_link_options(UVC_RP2040 PRIVATE "LINKER:--print-memory-usage")

pico_add_extra_outputs(UVC_RP2040)

