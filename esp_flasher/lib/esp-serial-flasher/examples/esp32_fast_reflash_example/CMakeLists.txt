# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS ../../)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-serial-flasher)

# There are issues with ESP-IDF 4.4 and -Wunused-parameter
if ("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "4.4")
    idf_component_get_property(flasher esp-serial-flasher COMPONENT_LIB)

    target_compile_options(${flasher}
    PRIVATE
        -Wunused-parameter
        -Wshadow
    )
endif()
