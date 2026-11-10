add_executable(test_desktop_icon_rsc test_desktop_icon_rsc.c
    ${PROJECT_SOURCE_DIR}/src/apps/desktop/desktop_icon_rsc.c)
target_include_directories(test_desktop_icon_rsc PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/apps/desktop)
target_compile_options(test_desktop_icon_rsc PRIVATE -Wall -Wextra -pedantic
    -UNDEBUG $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_desktop_icon_rsc PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
add_test(NAME test_desktop_icon_rsc COMMAND test_desktop_icon_rsc)
set_tests_properties(test_desktop_icon_rsc PROPERTIES LABELS unit)

add_executable(test_desktop_icon_lookup test_desktop_icon_lookup.c
    ${PROJECT_SOURCE_DIR}/src/apps/desktop/desktop_icon_lookup.c
    ${PROJECT_SOURCE_DIR}/src/apps/desktop/desktop_icon_rsc.c
    ${PROJECT_SOURCE_DIR}/src/apps/desktop/desktop_assets.c
    ${PROJECT_SOURCE_DIR}/src/apps/desktop/browser_view.c)
target_include_directories(test_desktop_icon_lookup PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/gem
    ${PROJECT_SOURCE_DIR}/src/apps/desktop)
# Retain real browser icon selection without linking unrelated GUI handlers.
target_compile_options(test_desktop_icon_lookup PRIVATE -Wall -Wextra -pedantic
    -UNDEBUG -ffunction-sections -fdata-sections
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_desktop_icon_lookup PRIVATE -Wl,--gc-sections
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
add_test(NAME test_desktop_icon_lookup COMMAND test_desktop_icon_lookup)
set_tests_properties(test_desktop_icon_lookup PROPERTIES LABELS unit)
