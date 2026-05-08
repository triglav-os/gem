add_library(gem SHARED
    client.c
)

target_include_directories(gem
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(gem PUBLIC platform_linux)

target_compile_features(gem PUBLIC c_std_11)

target_compile_options(gem
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(gem
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(gem
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
