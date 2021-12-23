cmake_minimum_required(VERSION 3.3)

project(imgui)

set(root ${CMAKE_CURRENT_SOURCE_DIR}/imgui_git)
set(rootStdLib ${root}/misc/cpp)
set(phdrs ${root}/imgui.h ${rootStdLib}/imgui_stdlib.h)
set(hdrs 
	${root}/imconfig.h 
	${root}/imgui_internal.h 
	${root}/imstb_rectpack.h 
	${root}/imstb_textedit.h 
	${root}/imstb_truetype.h
	${root}/misc/debuggers/imgui.natvis 
	)
set(srcs 
	${root}/imgui.cpp 
	${root}/imgui_demo.cpp 
	${root}/imgui_draw.cpp 
	${root}/imgui_tables.cpp 
	${root}/imgui_widgets.cpp 
	${rootStdLib}/imgui_stdlib.cpp)

add_library(imgui STATIC ${srcs} ${phdrs} ${hdrs})
target_include_directories(imgui PUBLIC "${root}")
target_include_directories(imgui INTERFACE "${rootStdLib}")
set_property(TARGET imgui PROPERTY FOLDER "3rd_party")
