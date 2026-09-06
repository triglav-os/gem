# Makes packaged ELF files resolve shared libraries from ../lib.

set(gemix_programs
    "${GEMIX_ROOT}/bin/calc"
    "${GEMIX_ROOT}/bin/clock"
    "${GEMIX_ROOT}/bin/desktop"
    "${GEMIX_ROOT}/bin/gemd"
    "${GEMIX_ROOT}/bin/terminal")
foreach(program IN LISTS gemix_programs)
    file(RPATH_SET FILE "${program}" NEW_RPATH "$ORIGIN/../lib")
endforeach()

file(GLOB gemix_libraries "${GEMIX_ROOT}/lib/*.so")
foreach(library IN LISTS gemix_libraries)
    file(RPATH_SET FILE "${library}" NEW_RPATH "$ORIGIN")
endforeach()
