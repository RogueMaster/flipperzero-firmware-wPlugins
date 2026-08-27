idf_component_register(
    SRCS "src/AsyncTCP.cpp"
    INCLUDE_DIRS "src"
    PRIV_REQUIRES esp_timer
)

target_compile_options(${COMPONENT_TARGET} PRIVATE -fno-rtti)
