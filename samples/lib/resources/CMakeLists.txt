add_library(sample_resources STATIC resources.c)
target_include_directories(sample_resources PUBLIC ${SAMPLES_ROOT}/include
    $<TARGET_PROPERTY:gem,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(sample_resources PRIVATE
    SAMPLE_BUILD_DATA="${GEM_SAMPLES_DIR}/data")
target_compile_options(sample_resources PRIVATE -Wall -Wextra -pedantic
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
