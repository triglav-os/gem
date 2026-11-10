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

# Desktop program icons: icon-only resources in the shared icon directory
# (share/gem/icons/<program>.rsc), built by resgen from the 32x32 reference
# artwork like the built-in icons.
set(gem_launcher_icons)
foreach(program gemscape terminal clock calc gemread ted)
    string(TOUPPER ${program} artwork)
    set(source ${PROJECT_SOURCE_DIR}/samples/data/artwork/icons/${artwork}.PNG)
    set(output ${GEM_RESOURCES_DIR}/icons/${program}.rsc)
    add_custom_command(OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GEM_RESOURCES_DIR}/icons
        COMMAND ${CMAKE_COMMAND} -E env
            ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0
            $<TARGET_FILE:resgen> icons -o ${output}
            ${source}
        DEPENDS resgen ${source}
        COMMENT "Generating ${program} desktop icon resource"
        VERBATIM)
    list(APPEND gem_launcher_icons ${output})
endforeach()
add_custom_target(gem_launcher_icons ALL DEPENDS ${gem_launcher_icons})
