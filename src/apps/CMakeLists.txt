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

# Shared program and location icons: icon-only resources in the shared icon
# directory (share/gem/icons/<name>.rsc), built by resgen from the 32x32 reference
# artwork like the built-in icons.
set(gem_launcher_icons)
foreach(icon gemscape terminal clock calc gemread ted home programs documents library)
    string(TOUPPER ${icon} artwork)
    set(source ${PROJECT_SOURCE_DIR}/samples/data/artwork/icons/${artwork}.PNG)
    set(output ${GEM_RESOURCES_DIR}/icons/${icon}.rsc)
    add_custom_command(OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GEM_RESOURCES_DIR}/icons
        COMMAND ${CMAKE_COMMAND} -E env
            ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0
            $<TARGET_FILE:resgen> icons -o ${output}
            ${source}
        DEPENDS resgen ${source}
        COMMENT "Generating ${icon} desktop icon resource"
        VERBATIM)
    list(APPEND gem_launcher_icons ${output})
endforeach()
# File-type icons: icons/.<ext>.rsc applies to every file with that extension.
foreach(extension txt md pdf)
    string(TOUPPER ${extension} artwork)
    set(source ${PROJECT_SOURCE_DIR}/samples/data/artwork/icons/${artwork}.PNG)
    set(output ${GEM_RESOURCES_DIR}/icons/.${extension}.rsc)
    add_custom_command(OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GEM_RESOURCES_DIR}/icons
        COMMAND ${CMAKE_COMMAND} -E env
            ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0
            $<TARGET_FILE:resgen> icons -o ${output}
            ${source}
        DEPENDS resgen ${source}
        COMMENT "Generating .${extension} file-type icon resource"
        VERBATIM)
    list(APPEND gem_launcher_icons ${output})
endforeach()
add_custom_target(gem_launcher_icons ALL DEPENDS ${gem_launcher_icons})
