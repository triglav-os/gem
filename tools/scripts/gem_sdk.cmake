# Export only public GEM headers, libraries and runtime resources.
file(MAKE_DIRECTORY "${SDK_ROOT}/include/gem" "${SDK_ROOT}/include/platform"
    "${SDK_ROOT}/lib" "${SDK_ROOT}/share/gem")
file(COPY "${SOURCE_ROOT}/include/gem.h" DESTINATION "${SDK_ROOT}/include")
file(COPY "${SOURCE_ROOT}/include/gem/" DESTINATION "${SDK_ROOT}/include/gem")
file(COPY "${SOURCE_ROOT}/include/platform/os.h" DESTINATION "${SDK_ROOT}/include/platform")
foreach(library IN LISTS GEM_LIBRARIES)
    file(COPY "${library}" DESTINATION "${SDK_ROOT}/lib")
endforeach()
foreach(resource fonts cursors.rsc alert_icons.rsc)
    file(COPY "${RESOURCE_ROOT}/${resource}" DESTINATION "${SDK_ROOT}/share/gem")
endforeach()

file(WRITE "${SDK_ROOT}/gem_sdk_config.cmake"
    "set(GEM_SDK_PLATFORM ${GEM_PLATFORM})\n")

file(MAKE_DIRECTORY "${SDK_ROOT}/bin")
file(COPY "${RESGEN}" DESTINATION "${SDK_ROOT}/bin")

# Remove sample assets exported by older SDK layouts during an in-place upgrade.
foreach(stale desktop_icons.rsc clock_numbers.rsc gemscape_toolbar.rsc maestro_tree.rsc)
    file(REMOVE "${SDK_ROOT}/share/gem/${stale}")
endforeach()
