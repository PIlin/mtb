cmake_minimum_required(VERSION 3.3)

project(imgui)

set(root ${CMAKE_CURRENT_SOURCE_DIR}/imgui_git)
set(phdrs ${root}/imgui.h)
set(hdrs ${root}/stb_rect_pack.h ${root}/stb_textedit.h ${root}/stb_truetype.h)
set(srcs ${root}/imgui.cpp ${root}/imgui_draw.cpp)

add_library(imgui STATIC ${srcs} ${phdrs} ${hdrs})
target_include_directories(imgui INTERFACE "${root}")
set_property(TARGET imgui PROPERTY FOLDER "3rd_party")