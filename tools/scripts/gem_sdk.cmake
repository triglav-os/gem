# Export only public GEM headers, libraries and runtime resources.
# This is a generated export, never an installation prefix. Discard stale files.
get_filename_component(sdk_name "${SDK_ROOT}" NAME)
if(NOT sdk_name STREQUAL "sdk" OR IS_SYMLINK "${SDK_ROOT}")
    message(FATAL_ERROR "SDK_ROOT must be a generated sdk directory")
endif()
# Fail before touching an existing export if a required input is missing.
foreach(input IN LISTS GEM_LIBRARIES)
    if(NOT EXISTS "${input}")
        message(FATAL_ERROR "Missing SDK library: ${input}")
    endif()
endforeach()
foreach(input "${RESGEN}" "${RESOURCE_ROOT}/fonts" "${RESOURCE_ROOT}/cursors.rsc"
              "${RESOURCE_ROOT}/alert_icons.rsc" "${SOURCE_ROOT}/include/gem.h")
    if(NOT EXISTS "${input}")
        message(FATAL_ERROR "Missing SDK input: ${input}")
    endif()
endforeach()
file(REMOVE_RECURSE "${SDK_ROOT}")
file(COPY "${SOURCE_ROOT}/LICENSE" DESTINATION "${SDK_ROOT}")
file(MAKE_DIRECTORY "${SDK_ROOT}/include/gem" "${SDK_ROOT}/include/platform"
    "${SDK_ROOT}/lib" "${SDK_ROOT}/share/gem")
file(COPY "${SOURCE_ROOT}/include/gem.h" DESTINATION "${SDK_ROOT}/include")
file(COPY "${SOURCE_ROOT}/include/gem/" DESTINATION "${SDK_ROOT}/include/gem")
file(COPY "${SOURCE_ROOT}/include/platform/os.h" DESTINATION "${SDK_ROOT}/include/platform")
foreach(component fat12 msa)
    file(COPY "${SOURCE_ROOT}/include/${component}" DESTINATION "${SDK_ROOT}/include")
endforeach()
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

file(WRITE "${SDK_ROOT}/lib/pkgconfig/gem.pc"
    "prefix=\${pcfiledir}/../..\nlibdir=\${prefix}/lib\nincludedir=\${prefix}/include\n\nName: GEM\nDescription: GEM application client API\nVersion: 0.1\nLibs: -L\${libdir} -lgem -lplatform_linux\nCflags: -I\${includedir}\n")
