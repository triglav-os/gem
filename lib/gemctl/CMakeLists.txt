add_library(gemctl STATIC
    gemctl.c
)

set_target_properties(gemctl PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(gemctl
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(gemctl PUBLIC c_std_11)

target_compile_options(gemctl
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)
