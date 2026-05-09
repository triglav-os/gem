add_library(fat12 SHARED
    fat12.c
)

set_target_properties(fat12 PROPERTIES
    OUTPUT_NAME fat12
)

target_include_directories(fat12
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(fat12 PUBLIC c_std_11)

target_compile_options(fat12
    PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(fat12
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
