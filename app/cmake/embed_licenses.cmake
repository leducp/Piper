# Inputs:
#   OUTPUT:    path to write the generated .cc
#   LICENSES:  semicolon-separated list of .txt paths

set(content "// Auto-generated -- do not edit. See app/cmake/embed_licenses.cmake.\n")
string(APPEND content "#include \"piper/app/bundled_licenses.h\"\n\n")
string(APPEND content "namespace piper::app\n{\n")

set(entries "")
foreach(lic IN LISTS LICENSES)
    get_filename_component(fname "${lic}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${fname}" sym)
    file(READ "${lic}" body)
    string(APPEND content "    static char const text_${sym}[] = R\"PIPER_LICENSE(${body})PIPER_LICENSE\";\n\n")
    string(APPEND entries "        { \"${fname}\", text_${sym} },\n")
endforeach()

if(entries STREQUAL "")
    string(APPEND content "    std::span<BundledLicense const> bundled_licenses()\n    {\n        return {};\n    }\n")
else()
    string(APPEND content "    static BundledLicense const table[] = {\n${entries}    };\n\n")
    string(APPEND content "    std::span<BundledLicense const> bundled_licenses()\n    {\n        return std::span<BundledLicense const>{ table, sizeof(table) / sizeof(BundledLicense) };\n    }\n")
endif()

string(APPEND content "}\n")

file(WRITE "${OUTPUT}" "${content}")
