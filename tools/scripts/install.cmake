# Runtime-only GNUInstallDirs installation. Development files belong in gem_sdk.
set(gem_install_programs gemd desktop terminal calc_gemd clock_gemd stout gemread gemscape ted)
set(gem_install_libraries gem aes vdi platform_linux fat12 msa prg)
if(GEM_PLATFORM STREQUAL "rasta")
    list(APPEND gem_install_libraries rasta)
endif()
foreach(library gem aes vdi platform_linux rasta fat12 msa prg)
    set_target_properties(${library} PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN")
endforeach()
install(TARGETS ${gem_install_programs} ${gem_install_libraries}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
# Desktop launcher icons live beside the programs they represent.
foreach(program gemscape terminal clock calc gemread ted)
    install(FILES ${GEM_APPS_DIR}/${program}.rsc DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()
install(DIRECTORY ${GEM_RESOURCES_DIR}/fonts DESTINATION ${CMAKE_INSTALL_DATADIR}/gem)
install(FILES ${GEM_RESOURCES_DIR}/cursors.rsc ${GEM_RESOURCES_DIR}/alert_icons.rsc
    ${GEM_SAMPLES_DIR}/data/gemscape_toolbar.rsc
    DESTINATION ${CMAKE_INSTALL_DATADIR}/gem)
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/gem)
install(FILES ${PROJECT_SOURCE_DIR}/src/apps/gemread/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/gemread)
install(FILES ${PROJECT_SOURCE_DIR}/docs/guides/PACKAGING.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR})
# Explicit runtime allowlist: one copy of each resource, beside libcef.so.
# Retain ANGLE/SwiftShader for browser canvas/WebGL software rendering.
set(gem_cef_root ${CMAKE_BINARY_DIR}/deps/cef/source)
set(gem_cef_binaries libcef.so libEGL.so libGLESv2.so libvk_swiftshader.so libvulkan.so.1)
foreach(file IN LISTS gem_cef_binaries)
    install(FILES ${gem_cef_root}/Release/${file} DESTINATION ${CMAKE_INSTALL_LIBDIR}/gem/cef)
endforeach()
install(FILES ${gem_cef_root}/Release/v8_context_snapshot.bin
    ${gem_cef_root}/Release/vk_swiftshader_icd.json
    ${gem_cef_root}/Resources/icudtl.dat
    ${gem_cef_root}/Resources/chrome_100_percent.pak
    ${gem_cef_root}/Resources/chrome_200_percent.pak
    ${gem_cef_root}/Resources/resources.pak
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/gem/cef)
install(FILES ${gem_cef_root}/Resources/locales/${GEM_CEF_LOCALE}.pak
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/gem/cef/locales)
install(FILES ${gem_cef_root}/LICENSE.txt ${gem_cef_root}/CREDITS.html
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/gemscape)
include(${PROJECT_SOURCE_DIR}/tools/scripts/package_notices.cmake)
if(GEM_PLATFORM STREQUAL "rasta")
    install(PROGRAMS ${GEM_TOOLS_DIR}/rasta DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
# Imported binaries are installed as files, so CMake does not strip them itself.
set(gem_strip_files)
foreach(file IN LISTS gem_cef_binaries)
    list(APPEND gem_strip_files "${CMAKE_INSTALL_LIBDIR}/gem/cef/${file}")
endforeach()
if(GEM_PLATFORM STREQUAL "rasta")
    list(APPEND gem_strip_files "${CMAKE_INSTALL_BINDIR}/rasta")
endif()
foreach(file IN LISTS gem_strip_files)
    install(CODE "if(CMAKE_INSTALL_DO_STRIP)\n  execute_process(COMMAND \"${CMAKE_STRIP}\" --strip-unneeded \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${file}\" COMMAND_ERROR_IS_FATAL ANY)\nendif()")
endforeach()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_target(gemix_package
    COMMAND ${Python3_EXECUTABLE} -B ${PROJECT_SOURCE_DIR}/tools/scripts/package_stage.py
        --build ${CMAKE_BINARY_DIR} --destination ${GEM_OUTPUT_ROOT}/gemix
    DEPENDS ${gem_install_programs} ${gem_install_libraries} gem_runtime_fonts
        gem_runtime_cursors gem_runtime_alert_icons gem_runtime_gemscape_toolbar
        gem_launcher_icons
    COMMENT "Staging the minimal GEM runtime package"
    VERBATIM)
if(GEM_PLATFORM STREQUAL "rasta")
    add_dependencies(gemix_package rasta_viewer)
endif()
