# Inputs:
#   OUTPUT: path to write the generated .cc
#   SOURCE: path to the theme file

file(READ "${SOURCE}" theme_json)
set(generated_source "// Auto-generated -- do not edit. See app/cmake/embed_theme.cmake.\n")
string(APPEND generated_source "#include \"piper/app/bundled_theme.h\"\n\n")
string(APPEND generated_source "namespace piper::studio\n{\n")
string(APPEND generated_source "    char const* bundled_theme()\n    {\n")
string(APPEND generated_source "        static char const text[] = R\"PIPER_THEME(${theme_json})PIPER_THEME\";\n")
string(APPEND generated_source "        return text;\n")
string(APPEND generated_source "    }\n")
string(APPEND generated_source "}\n")
file(WRITE "${OUTPUT}" "${generated_source}")
