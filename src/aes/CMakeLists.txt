add_library(aes STATIC
    _aes.c
    aes.c
)

target_include_directories(aes
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(aes PUBLIC vdi)

target_compile_features(aes PUBLIC c_std_11)

target_compile_options(aes
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(aes
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(aes
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
