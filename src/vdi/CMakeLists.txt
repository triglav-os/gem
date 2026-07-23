file(GLOB vdi_sources CONFIGURE_DEPENDS *.c)

add_library(vdi SHARED ${vdi_sources})

add_dependencies(vdi gem_runtime_fonts gem_runtime_cursors)

target_include_directories(vdi
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

if(GEM_PLATFORM STREQUAL "linux")
    target_link_libraries(vdi PUBLIC platform_linux m)
else()
    target_link_libraries(vdi PUBLIC rasta m)
endif()

target_compile_features(vdi PUBLIC c_std_11)

target_compile_options(vdi
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(vdi
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
