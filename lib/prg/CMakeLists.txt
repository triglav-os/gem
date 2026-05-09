add_library(prg SHARED
    prg.c
)

set_target_properties(prg PROPERTIES
    OUTPUT_NAME prg
)

target_include_directories(prg
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(prg PUBLIC c_std_11)

target_compile_options(prg
    PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(prg
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
