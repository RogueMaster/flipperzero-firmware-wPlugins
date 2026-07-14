cmake_minimum_required(VERSION 3.15)

cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0117 NEW)

project(minigolf)
if(EMSCRIPTEN)
    option(EMSCRIPTEN_OPTIMIZED "Produce size-optimized emscripten build" ON)
    if(EMSCRIPTEN_OPTIMIZED)
        set(EXTRA_FLAGS "${EXTRA_FLAGS} -Oz")
        set(EXTRA_FLAGS "${EXTRA_FLAGS} -flto")
        set(EXTRA_FLAGS "${EXTRA_FLAGS} --closure 1")
        set(EXTRA_FLAGS "${EXTRA_FLAGS} -fno-rtti -fno-exceptions")
        add_link_options("-s MODULARIZE=1")
        add_link_options("-s ALLOW_UNIMPLEMENTED_SYSCALLS=0")
        add_link_options("-flto")
    endif()
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -Wall")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -Wno-unused-command-line-argument")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -Wno-unused-function")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -s ASYNCIFY")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -sEXPORTED_FUNCTIONS=_main,_initpers")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -sEXPORTED_RUNTIME_METHODS=ccall,FS")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${EXTRA_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_FLAGS}")
    add_link_options("-lidbfs.js")
endif()

set(PRE_CONFIGURE_FILE "git.hpp.in")
# write to source folder cause arduino ide :(
set(POST_CONFIGURE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/git.hpp")
include(git_watcher.cmake)
add_library(git INTERFACE ${POST_CONFIGURE_FILE})
target_include_directories(git INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(git check_git)

set(WINFLAG)
if(WIN32)
    set(WINFLAG WIN32)
endif()

set(MINIGOLF_SOURCES
    game.hpp
    camera.cpp
    div.cpp
    font.cpp
    mul.cpp
    game.hpp
    draw.cpp
    graphics.cpp
    physics.cpp
    sincos.cpp
    render_scene.cpp
    mat.cpp
    game.cpp
    save.cpp
)

add_executable(gen_fxsave tools/gen_fxsave.cpp game.hpp save.cpp)
target_include_directories(gen_fxsave PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_definitions(gen_fxsave PUBLIC -DARDUGOLF_FX=1 -DGEN_FXSAVE=1)
set(FXSAVE_BIN "${CMAKE_CURRENT_SOURCE_DIR}/fxsave.bin")
add_custom_command(
    OUTPUT "${FXSAVE_BIN}"
    COMMAND gen_fxsave "${FXSAVE_BIN}"
    DEPENDS gen_fxsave
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

find_package(Python3 REQUIRED)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/fxdata.bin"
    COMMAND "${Python3_EXECUTABLE}" "fxdata-build.py" "fxdata.txt"
    DEPENDS "fxdata.txt"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

add_executable(bin2c tools/bin2c.c)
set(FX_DATA_HEADER "${CMAKE_CURRENT_BINARY_DIR}/FX_DATA.hpp")
add_custom_command(
    OUTPUT "${FX_DATA_HEADER}"
    COMMAND bin2c "${CMAKE_CURRENT_SOURCE_DIR}/fxdata.bin" "${FX_DATA_HEADER}" "FX_DATA"
    DEPENDS bin2c "${CMAKE_CURRENT_SOURCE_DIR}/fxdata.bin"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
list(APPEND MINIGOLF_SOURCES "${FX_DATA_HEADER}")
set(FX_SAVE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/FX_SAVE.hpp")
add_custom_command(
    OUTPUT "${FX_SAVE_HEADER}"
    COMMAND bin2c "${FXSAVE_BIN}" "${FX_SAVE_HEADER}" "FX_SAVE"
    DEPENDS bin2c "${FXSAVE_BIN}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
list(APPEND MINIGOLF_SOURCES "${FX_SAVE_HEADER}")

add_executable(
    ${PROJECT_NAME} ${WINFLAG}
    README.md
    .editorconfig
    main_emscripten.cpp
    main_win32.cpp
    ${MINIGOLF_SOURCES}
)
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}"
)

if(MSVC)

    add_executable(
        ardugolf_editor
        tools/level_editor.cpp
        tools/imgui/backends/imgui_impl_win32.cpp
        tools/imgui/backends/imgui_impl_win32.h
        tools/imgui/backends/imgui_impl_dx9.h
        tools/imgui/backends/imgui_impl_dx9.cpp
        tools/nativefiledialog/nfd_win.cpp
        tools/nativefiledialog/nfd_common.c
        tools/nativefiledialog/nfd_common.h
        tools/nativefiledialog/common.h
        tools/nativefiledialog/include/nfd.h
        ${MINIGOLF_SOURCES}
    )
    target_include_directories(ardugolf_editor PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/imgui"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/fmt"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/nativefiledialog/include"
        "${CMAKE_CURRENT_BINARY_DIR}"
    )
    set_property(TARGET ardugolf_editor PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(WIN32)
        string(REGEX REPLACE "/" "\\\\\\\\" BASE_DIR "${BASE_DIR}")
    endif()
    target_compile_definitions(ardugolf_editor PUBLIC
        -DBASE_DIR="${BASE_DIR}" -DARDUGOLF_FX=0)

endif()

target_link_libraries(${PROJECT_NAME} PUBLIC git)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED 11)

if(NOT EMSCRIPTEN)
    add_executable(gen_math_tables tools/gen_math_tables.cpp)
    add_executable(test_math tools/test_math.cpp ${MINIGOLF_SOURCES})
    target_include_directories(gen_math_tables PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}"
    )
    target_include_directories(test_math PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()

if(MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_options(${PROJECT_NAME} PUBLIC
        /DEBUG
        /MAP
        /EMITPOGOPHASEINFO
        /MANIFEST:NO
        /DYNAMICBASE:NO
        /FIXED
        /INCREMENTAL:NO
        /OPT:REF
        /OPT:ICF=16
        /SAFESEH:NO
        $<$<NOT:$<CONFIG:Debug>>:/ENTRY:main>
        $<$<NOT:$<CONFIG:Debug>>:/NODEFAULTLIB>
    )
    target_compile_definitions(${PROJECT_NAME} PUBLIC -D_HAS_EXCEPTIONS=0)
    target_compile_options(${PROJECT_NAME} PUBLIC /GR- /GS-)
    #if(CMAKE_CXX_FLAGS MATCHES "/EHsc ")
    #    string(REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    #endif()
    #if(CMAKE_CXX_FLAGS MATCHES "/EHsc$")
    #    string(REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    #endif()
endif()
