# Inputs:
#   OUTPUT: path to write the generated .cc
#   SOURCE: path to the project license file

file(READ "${SOURCE}" body)
set(content "// Auto-generated -- do not edit. See app/cmake/embed_project_license.cmake.\n")
string(APPEND content "#include \"piper/app/project_license.h\"\n\n")
string(APPEND content "namespace piper::studio\n{\n")
string(APPEND content "    char const* project_license()\n    {\n")
string(APPEND content "        static char const text[] = R\"PIPER_LICENSE(${body})PIPER_LICENSE\";\n")
string(APPEND content "        return text;\n")
string(APPEND content "    }\n")
string(APPEND content "}\n")
file(WRITE "${OUTPUT}" "${content}")
