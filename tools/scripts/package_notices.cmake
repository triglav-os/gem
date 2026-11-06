# Notices are runtime requirements too; do not ship source trees to carry them.
set(gem_notice_dir ${CMAKE_BINARY_DIR}/package-notices)
file(MAKE_DIRECTORY ${gem_notice_dir})
file(READ ${GEM_MUSASHI_SOURCE}/readme.txt gem_musashi_readme)
string(FIND "${gem_musashi_readme}" "LICENSE AND COPYRIGHT:" gem_notice_start)
string(FIND "${gem_musashi_readme}" "AVAILABILITY:" gem_notice_end)
math(EXPR gem_notice_length "${gem_notice_end} - ${gem_notice_start}")
string(SUBSTRING "${gem_musashi_readme}" ${gem_notice_start} ${gem_notice_length} gem_notice)
file(WRITE ${gem_notice_dir}/Musashi.txt "${gem_notice}")
file(READ ${GEM_MUSASHI_SOURCE}/softfloat/softfloat.c gem_softfloat_source)
string(FIND "${gem_softfloat_source}" "*/" gem_notice_end)
math(EXPR gem_notice_length "${gem_notice_end} + 2")
string(SUBSTRING "${gem_softfloat_source}" 0 ${gem_notice_length} gem_notice)
file(WRITE ${gem_notice_dir}/SoftFloat.txt "${gem_notice}\n")
install(FILES ${gem_notice_dir}/Musashi.txt ${gem_notice_dir}/SoftFloat.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/stout)
set(gem_mupdf_root ${CMAKE_BINARY_DIR}/deps/mupdf/source)
# These components occur in the pinned libmupdf-third build; fonts are embedded.
set(gem_mupdf_notices
    COPYING
    thirdparty/brotli/LICENSE thirdparty/extract/COPYING
    thirdparty/freetype/LICENSE.TXT thirdparty/freetype/docs/FTL.TXT
    thirdparty/freetype/docs/GPLv2.TXT thirdparty/gumbo-parser/COPYING
    thirdparty/harfbuzz/COPYING thirdparty/jbig2dec/LICENSE
    thirdparty/lcms2/LICENSE thirdparty/libjpeg/README thirdparty/mujs/COPYING
    thirdparty/openjpeg/LICENSE thirdparty/zlib/LICENSE
    resources/fonts/urw/OFL.txt resources/fonts/noto/COPYING
    resources/fonts/sil/OFL.txt resources/fonts/han/LICENSE.txt)
foreach(notice IN LISTS gem_mupdf_notices)
    get_filename_component(directory ${notice} DIRECTORY)
    install(FILES ${gem_mupdf_root}/${notice}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/mupdf/${directory})
endforeach()
if(GEM_PLATFORM STREQUAL "rasta")
    install(FILES ${PROJECT_SOURCE_DIR}/docs/licenses/RASTA.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/rasta)
endif()
