# Inputs (set via cmake -D ... -P):
#   OUTPUT: path to write the generated .cc
#   FONTS:  semicolon-separated list of TTF/OTF paths (may be empty)

set(content "// Auto-generated -- do not edit. See app/cmake/embed_fonts.cmake.\n")
string(APPEND content "#include \"piper/app/bundled_fonts.h\"\n\n")
string(APPEND content "namespace piper::studio\n{\n")

set(entries "")
foreach(font IN LISTS FONTS)
    get_filename_component(fname "${font}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${fname}" sym)
    file(READ "${font}" hex HEX)
    string(LENGTH "${hex}" hex_len)
    math(EXPR byte_count "${hex_len} / 2")
    string(REGEX REPLACE "(..)" "0x\\1," hex "${hex}")
    string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n        " hex "${hex}")
    string(APPEND content "    static unsigned char const data_${sym}[] = {\n        ${hex}\n    };\n\n")
    string(APPEND entries "        { \"${fname}\", data_${sym}, ${byte_count}u },\n")
endforeach()

if(entries STREQUAL "")
    string(APPEND content "    std::span<BundledFont const> bundled_fonts()\n    {\n        return {};\n    }\n")
else()
    string(APPEND content "    static BundledFont const table[] = {\n${entries}    };\n\n")
    string(APPEND content "    std::span<BundledFont const> bundled_fonts()\n    {\n        return std::span<BundledFont const>{ table, sizeof(table) / sizeof(BundledFont) };\n    }\n")
endif()

string(APPEND content "}\n")

file(WRITE "${OUTPUT}" "${content}")
