# Bundled GEM applications. Independent SDK examples remain under samples/.
add_subdirectory(desktop)
add_subdirectory(terminal)
add_subdirectory(clock)
add_subdirectory(calculator)
add_subdirectory(gemscape)
add_subdirectory(gemread)
add_subdirectory(stout)

# Clock shares the artwork loader with the independent visual samples.
foreach(target clock clock_gemd gemscape gemscape_hosted)
    target_link_libraries(${target} PRIVATE sample_resources)
endforeach()
