cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
project(pi_pico_example C CXX ASM)

set(CMAKE_C_STANDARD 11)

pico_sdk_init()

include(${CMAKE_CURRENT_LIST_DIR}/../common/bin2array.cmake)
create_resources(${CMAKE_CURRENT_LIST_DIR}/../binaries/Hello-world binaries.c)

add_executable(${PROJECT_NAME}
    ../common/example_common.c
    binaries.c
    main.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ../common)

pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

pico_add_extra_outputs(${PROJECT_NAME})

set(PORT PI_PICO)
set(FLASHER_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
add_subdirectory(${FLASHER_DIR} ${CMAKE_BINARY_DIR}/flasher)

target_link_libraries(${PROJECT_NAME} PRIVATE flasher)
target_link_libraries(${PROJECT_NAME} PRIVATE pico_stdlib)
