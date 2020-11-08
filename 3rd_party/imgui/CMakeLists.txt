cmake_minimum_required(VERSION 3.3)

project(imgui)

set(root ${CMAKE_CURRENT_SOURCE_DIR}/imgui_git)
set(phdrs ${root}/imgui.h)
set(hdrs ${root}/imgui_internal.h ${root}/imstb_rectpack.h ${root}/imstb_textedit.h ${root}/imstb_truetype.h)
set(srcs ${root}/imgui.cpp ${root}/imgui_draw.cpp ${root}/imgui_widgets.cpp)

add_library(imgui STATIC ${srcs} ${phdrs} ${hdrs})
target_include_directories(imgui INTERFACE "${root}")
set_property(TARGET imgui PROPERTY FOLDER "3rd_party")
