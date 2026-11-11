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

# Every approved icon has an individual resource in share/gem/icons. Generic
# icons use Desktop's type names; extension icons retain their leading dot.
set(gem_launcher_icons)
foreach(artwork BLAISE CALC CLOCK DISK DOC DOCUMENTS FOLDER FYODOR GEMREAD
        GEMSCAPE HOME ISAAC LIBRARY MD PDF PRG PROGRAMS TED TERMINAL TRASH TXT)
    string(TOLOWER ${artwork} icon)
    if(artwork STREQUAL "DOC")
        set(icon document)
    elseif(artwork STREQUAL "PRG")
        set(icon program)
    elseif(artwork MATCHES "^(TXT|MD|PDF)$")
        set(icon .${icon})
    endif()
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
add_custom_target(gem_launcher_icons ALL DEPENDS ${gem_launcher_icons})
