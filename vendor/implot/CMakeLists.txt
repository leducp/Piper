find_package(imgui REQUIRED)

add_library(implot STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/implot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui::imgui)
target_compile_options(implot PRIVATE -Wno-conversion -Wno-cast-qual)
set_target_properties(implot PROPERTIES POSITION_INDEPENDENT_CODE ON)
