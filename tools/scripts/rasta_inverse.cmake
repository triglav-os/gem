# Reproducible GEM display-polarity patch for the pinned upstream Rasta archive.
# Keep the shared framebuffer unchanged; invert only decoded viewer colors.
function(replace_rasta relative before after)
    set(path "${SOURCE_DIR}/${relative}")
    file(READ "${path}" contents)
    string(FIND "${contents}" "${after}" patched)
    if(NOT patched EQUAL -1)
        return()
    endif()
    string(FIND "${contents}" "${before}" original)
    if(original EQUAL -1)
        message(FATAL_ERROR "Rasta inverse patch does not match ${relative}")
    endif()
    string(REPLACE "${before}" "${after}" contents "${contents}")
    file(WRITE "${path}" "${contents}")
endfunction()

replace_rasta(include/rasta_options.h
    "    bool show_cursor;"
    "    bool show_cursor;\n    bool inverse;")
replace_rasta(src/rasta_options.cpp
    "        .show_cursor = true,"
    "        .show_cursor = true,\n        .inverse = false,")
replace_rasta(src/rasta_options.cpp
    [=[        } else if (argument == "--cursor") {]=]
    [=[        } else if (argument == "--inverse") {
            options.inverse = true;
        } else if (argument == "--cursor") {]=])
replace_rasta(src/rasta_options.cpp
    [=[ [--cursor <on|off>]]=]
    [=[ [--cursor <on|off>] [--inverse]]=])
replace_rasta(include/raster_decoder.h
    "std::uint32_t bits_per_pixel, std::vector<std::uint8_t> &rgb24_buffer);"
    "std::uint32_t bits_per_pixel, std::vector<std::uint8_t> &rgb24_buffer,\n    bool inverse = false);")
replace_rasta(src/raster_decoder.cpp
    "std::uint32_t bits_per_pixel, std::vector<std::uint8_t> &rgb24_buffer)"
    "std::uint32_t bits_per_pixel, std::vector<std::uint8_t> &rgb24_buffer,\n    bool inverse)")
replace_rasta(src/raster_decoder.cpp
    "        const std::uint32_t value = reader.read_bits(bits_per_pixel);"
    "        const std::uint32_t value = reader.read_bits(bits_per_pixel) ^\n            (inverse ? (1U << bits_per_pixel) - 1U : 0U);")
replace_rasta(src/sdl_emulator.cpp
    "            rgb24_buffer);"
    "            rgb24_buffer, active_options.inverse);")
replace_rasta(tests/test_rasta.cpp
    [=[} // namespace

int main()]=]
    [=[bool test_gem_inverse_polarity()
{
    const char *argv[] = {"rasta", "--inverse"};
    rasta_options options {}, configured {};
    std::string error;
    if (!expect(!default_rasta_options().inverse, "normal mode stays default") ||
        !expect(parse_arguments(2, const_cast<char **>(argv), options, error),
            "inverse command-line option must parse") ||
        !expect(options.inverse, "inverse option must be enabled") ||
        !expect(parse_argument_string("--width 640 --height 400", options,
            configured, error) && configured.inverse,
            "GEM subscriber reconfiguration must retain inverse mode")) {
        return false;
    }
    const std::uint8_t raster[] = {0x80};
    std::vector<std::uint8_t> rgb24;
    decode_raster_to_rgb24(raster, 1, 2, 1, 1, rgb24, configured.inverse);
    return expect(rgb24 == std::vector<std::uint8_t>({0, 0, 0, 255, 255, 255}),
               "GEM set bit must display black ink; clear bit white paper") &&
        expect(raster[0] == 0x80, "display inversion must not alter GEM pixels");
}

} // namespace

int main()]=])
replace_rasta(tests/test_rasta.cpp
    "    if (!test_framebuffer_size()) {"
    "    if (!test_gem_inverse_polarity()) {\n        return EXIT_FAILURE;\n    }\n    if (!test_framebuffer_size()) {")
