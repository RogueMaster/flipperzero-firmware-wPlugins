cmake_minimum_required(VERSION 3.16)

project(flipcraft_sfml LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FLIPCRAFT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

add_executable(flipcraft-sfml
    main.cpp
    platform.cpp
    "${FLIPCRAFT_ROOT}/game/game.cpp"
    "${FLIPCRAFT_ROOT}/game/data.cpp"
    "${FLIPCRAFT_ROOT}/game/world.cpp"
    "${FLIPCRAFT_ROOT}/render/render.cpp"
    "${FLIPCRAFT_ROOT}/render/gui.cpp"
)

target_include_directories(flipcraft-sfml PRIVATE "${FLIPCRAFT_ROOT}")

find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

target_link_libraries(flipcraft-sfml PRIVATE SFML::Graphics SFML::Window SFML::System)
