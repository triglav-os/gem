file(GLOB gem_vdi_sources CONFIGURE_DEPENDS client_vdi_*.c client_aes_*.c)
add_library(gem SHARED
    ${gem_vdi_sources}
    bitmap_size.c
    vdi_validate.c
    tree_pack.c
    tree_decode.c
    aes_validate.c
    client.c
    client_queries.c
    protocol.c
    client_objc.c
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
