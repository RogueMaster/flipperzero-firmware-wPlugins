cmake_minimum_required(VERSION 3.5)

set(FLASHER_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set(PORT RASPBERRY_PI)

project(raspberry_flasher)

add_compile_definitions(SERIAL_FLASHER_INTERFACE_UART)

include(${CMAKE_CURRENT_LIST_DIR}/../common/bin2array.cmake)
create_resources(${CMAKE_CURRENT_LIST_DIR}/../binaries/Hello-world Src/binaries_flash.c)
create_resources(${CMAKE_CURRENT_LIST_DIR}/../binaries/RAM_APP Src/binaries_ram.c)

add_executable(${CMAKE_PROJECT_NAME} ../common/example_common.c Src/main.c Src/binaries_flash.c Src/binaries_ram.c)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ../common)

add_subdirectory(${FLASHER_DIR} ${CMAKE_BINARY_DIR}/flasher)

target_compile_options(flasher
PRIVATE
    -Wunused-parameter
    -Wshadow
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE flasher)
