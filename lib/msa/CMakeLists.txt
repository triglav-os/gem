add_library(msa SHARED
    msa.c
)

set_target_properties(msa PROPERTIES
    OUTPUT_NAME msa
)

target_include_directories(msa
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(msa PUBLIC c_std_11)

target_compile_options(msa
    PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(msa
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
