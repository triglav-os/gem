add_executable(resgen resgen.c)

set_target_properties(resgen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)

target_include_directories(resgen
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(resgen PRIVATE c_std_11)

target_compile_options(resgen
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(resgen
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(resgen
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

add_custom_command(OUTPUT ${GEM_RESOURCES_DIR}/cursors.rsc
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> cursors
        -t 01234567
        -o ${GEM_RESOURCES_DIR}/cursors.rsc
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/POINTER.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/IBEAM.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/BEE.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/POINTHAND.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/FLATHAND.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/THINCROSS.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/THICKCROSS.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/OUTLINECROSS.PNG
    DEPENDS
        resgen
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/POINTER.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/IBEAM.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/BEE.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/POINTHAND.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/FLATHAND.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/THINCROSS.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/THICKCROSS.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/cursors/OUTLINECROSS.PNG
    COMMENT "Generating GEM runtime cursor resource"
    VERBATIM)
add_custom_target(gem_runtime_cursors ALL DEPENDS ${GEM_RESOURCES_DIR}/cursors.rsc)

add_custom_command(OUTPUT ${GEM_RESOURCES_DIR}/alert_icons.rsc
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> icons
        -o ${GEM_RESOURCES_DIR}/alert_icons.rsc
        ${PROJECT_SOURCE_DIR}/docs/images/originals/icons/EXCLAMATION.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/icons/QUESTION.PNG
        ${PROJECT_SOURCE_DIR}/docs/images/originals/icons/STOP.PNG
    DEPENDS resgen ${PROJECT_SOURCE_DIR}/docs/images/originals/icons/EXCLAMATION.PNG ${PROJECT_SOURCE_DIR}/docs/images/originals/icons/QUESTION.PNG ${PROJECT_SOURCE_DIR}/docs/images/originals/icons/STOP.PNG
    COMMENT "Generating GEM alert icon resource"
    VERBATIM)
add_custom_target(gem_runtime_alert_icons ALL DEPENDS ${GEM_RESOURCES_DIR}/alert_icons.rsc)
