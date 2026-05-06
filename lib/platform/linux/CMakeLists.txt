add_library(platform_linux SHARED os.c)

target_include_directories(platform_linux
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(platform_linux PUBLIC c_std_11)

target_compile_options(platform_linux
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(platform_linux
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

