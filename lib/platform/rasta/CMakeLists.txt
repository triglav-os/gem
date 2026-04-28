file(GLOB rasta_sources CONFIGURE_DEPENDS *.c)

add_library(rasta STATIC ${rasta_sources})

target_include_directories(rasta
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(rasta PUBLIC c_std_11)

target_compile_options(rasta
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(rasta
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(rasta
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
