cmake_minimum_required(VERSION 3.10)
project(Piper)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
# Instruct CMake to create code from Qt designer ui files
set(CMAKE_AUTOUIC ON)

# Find the QtWidgets library
find_package(Qt5Widgets CONFIG REQUIRED)

set(piper_lib_src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Scene.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/View.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Node.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Attribute.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AttributeMember.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Link.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NodeCreator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CreatorPopup.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JsonExport.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PropertyDelegate.cc
)

set(piper_editor_src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

# Tell CMake to create the helloworld executable
add_library(piper ${piper_lib_src})
target_include_directories(piper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(piper Qt5::Widgets)

add_executable(editor ${piper_editor_src})
target_link_libraries(editor piper)

# Install the executable
install(TARGETS piper DESTINATION bin)
