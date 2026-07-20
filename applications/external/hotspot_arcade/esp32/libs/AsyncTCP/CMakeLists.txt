set(COMPONENT_SRCDIRS
    "src"
)

set(COMPONENT_ADD_INCLUDEDIRS
    "src"
)

set(COMPONENT_REQUIRES
    "arduino-esp32"
)

register_component()

target_compile_options(${COMPONENT_TARGET} PRIVATE -fno-rtti)
