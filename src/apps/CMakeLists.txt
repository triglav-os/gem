# Bundled GEM applications. Independent SDK examples remain under samples/.
add_subdirectory(desktop)
add_subdirectory(terminal)
add_subdirectory(clock)
add_subdirectory(calculator)
add_subdirectory(gemscape)
add_subdirectory(gemread)
add_subdirectory(stout)
add_subdirectory(ted)

# Clock shares the artwork loader with the independent visual samples.
foreach(target clock clock_gemd gemscape gemscape_hosted)
    target_link_libraries(${target} PRIVATE sample_resources)
endforeach()

# Desktop launcher icons: icon-only resources beside their programs, built
# by resgen from the 32x32 reference artwork like the built-in icons.
set(gem_launcher_icons)
foreach(program gemscape terminal clock calc gemread ted)
    string(TOUPPER ${program} artwork)
    set(source ${PROJECT_SOURCE_DIR}/samples/data/artwork/icons/${artwork}.PNG)
    add_custom_command(OUTPUT ${GEM_APPS_DIR}/${program}.rsc
        COMMAND ${CMAKE_COMMAND} -E env
            ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0
            $<TARGET_FILE:resgen> icons -o ${GEM_APPS_DIR}/${program}.rsc
            ${source}
        DEPENDS resgen ${source}
        COMMENT "Generating ${program} desktop icon resource"
        VERBATIM)
    list(APPEND gem_launcher_icons ${GEM_APPS_DIR}/${program}.rsc)
endforeach()
add_custom_target(gem_launcher_icons ALL DEPENDS ${gem_launcher_icons})
