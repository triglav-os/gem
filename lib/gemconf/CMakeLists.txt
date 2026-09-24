add_library(gemconf STATIC gemconf.c)

set_target_properties(gemconf PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(gemconf
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(gemconf PUBLIC c_std_11)

target_compile_options(gemconf
    PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
