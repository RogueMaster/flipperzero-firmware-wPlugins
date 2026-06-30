set(srcs main.c ../../common/example_common.c)
set(include_dirs . ../../common)

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs})
set(target ${COMPONENT_LIB})
