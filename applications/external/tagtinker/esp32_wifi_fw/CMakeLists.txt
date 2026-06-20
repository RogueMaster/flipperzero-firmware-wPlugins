# TagTinker WiFi firmware - ESP-IDF top-level CMake.
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS shared)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tagtinker_wifi)
