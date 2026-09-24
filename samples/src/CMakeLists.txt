add_subdirectory(maestro)
add_subdirectory(msa)

foreach(target maestro maestro_hosted)
    target_link_libraries(${target} PRIVATE sample_resources)
endforeach()
