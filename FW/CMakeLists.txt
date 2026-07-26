cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(rc_car_control C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


if (PICO_SDK_VERSION_STRING VERSION_LESS "2.1.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.1.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# If you want debug output from USB (pass -DPICO_STDIO_USB=1) this ensures you don't lose any debug output while USB is set up
#if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
#    set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
#endif()

# Initialize the SDK
#set(pioasm_DIR "f:/_Work/Programing/rpi/pioasm")
set(picotool_DIR "f:/_work/_Programing/rpi/picotool")
pico_sdk_init()

#targetname
set(TRGT car_lighting)

add_executable(${TRGT} main.c
        turn_light.c
        ibus.c
        rear_light.c
        beacons.c
        beacons.h
        to_mfc.c
        to_mfc.h
        lights.c
        lights.h
        switch.h
        aux1.c
        aux1.h
)

# pull in common dependencies
target_link_libraries(${TRGT}
        pico_stdlib
        hardware_pwm
        hardware_adc)


pico_enable_stdio_usb(${TRGT} 0)
pico_enable_stdio_uart(${TRGT} 0)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        )
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()

set_property(TARGET ${TRGT} APPEND_STRING PROPERTY LINK_FLAGS
        "-Wl,--print-memory-usage"
)



# create map/bin/hex file etc.
pico_add_extra_outputs(${TRGT})

# add url via pico_set_program_url
#example_auto_set_url(blink)

add_custom_command(TARGET ${TRGT} POST_BUILD
        COMMAND arm-none-eabi-size --format=barkley --totals ${TRGT}".elf"
        COMMENT "Invoking: Cross ARM GNU Print Size")
