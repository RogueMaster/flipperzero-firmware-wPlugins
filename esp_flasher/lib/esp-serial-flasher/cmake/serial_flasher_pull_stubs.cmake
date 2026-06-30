macro(serial_flasher_pull_stubs)
    set(ONE_VALUE_KEYWORDS VERSION SOURCE PATH_OVERRIDE)
    cmake_parse_arguments(FLASHER_STUB "" "${ONE_VALUE_KEYWORDS}" "" "${ARGN}")

    # Don't make this mandatory, some users might not have Python installed
    find_package(Python COMPONENTS Interpreter)
    if(NOT Python_Interpreter_FOUND)
        message(WARNING "Python not found, cannot run gen_stub_sources.py")
        return()
    endif()

    # By default use the esp-flasher-stub project stubs
    if (NOT DEFINED FLASHER_STUB_SOURCE)
        set(FLASHER_STUB_SOURCE "https://github.com/esp-rs/esp-flasher-stub/releases/download")
    endif()

    execute_process(
    COMMAND
        ${Python_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_stub_sources.py
        ${FLASHER_STUB_VERSION}
        ${FLASHER_STUB_SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${FLASHER_STUB_PATH_OVERRIDE}
    )
endmacro()
