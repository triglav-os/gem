# Build the latest upstream main on every invocation, never a sibling checkout.
include(ExternalProject)
find_package(Git REQUIRED)
set(GEM_RASTA_SOURCE_DIR "${CMAKE_BINARY_DIR}/deps/rasta-source")
ExternalProject_Add(rasta_viewer
    GIT_REPOSITORY "https://github.com/tstih/rasta.git"
    GIT_TAG origin/main
    GIT_SHALLOW TRUE
    GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
    GIT_CONFIG http.sslVerify=true
    TLS_VERIFY TRUE
    # Always fetch, even if the parent project disables dependency updates.
    UPDATE_DISCONNECTED FALSE
    PREFIX "${CMAKE_BINARY_DIR}/deps/rasta"
    SOURCE_DIR "${GEM_RASTA_SOURCE_DIR}"
    BINARY_DIR "${CMAKE_BINARY_DIR}/deps/rasta-build"
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc
        -DCMAKE_CXX_COMPILER=g++
    # Recheck outputs incrementally, including after bin/ has been removed.
    # Git updates first; unchanged source keeps its compiled objects.
    BUILD_ALWAYS TRUE
    BUILD_COMMAND ${CMAKE_COMMAND} -E env --unset=MAKEFLAGS --unset=MFLAGS
        --unset=GNUMAKEFLAGS ${CMAKE_COMMAND} --build <BINARY_DIR>
        --target rasta test_rasta --parallel 4
    INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory "${GEM_TOOLS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            <SOURCE_DIR>/bin/rasta "${GEM_TOOLS_DIR}/rasta"
        COMMAND ${GIT_EXECUTABLE} -C <SOURCE_DIR> log -1
            "--format=Rasta upstream revision: %H"
    BUILD_BYPRODUCTS "${GEM_TOOLS_DIR}/rasta")
