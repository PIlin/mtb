cmake_minimum_required(VERSION 3.2)

if (${CMAKE_GENERATOR_TOOLSET} MATCHES "LLVM-vs")
	set(MTB_CLANG_CL ON)
endif()


if (${MTB_CLANG_CL})
	add_compile_options(-Wno-unused-command-line-argument)
	# clang-cl silently treats /Zi as /Zi and doesn't output pdb's. MSBuild forces recompilation every time in such case.
	# See https://bugs.llvm.org/show_bug.cgi?id=36140
	add_compile_options(/Z7) 

	option(MTB_ASAN "Build MTB with ASAN" OFF)
	if (${MTB_ASAN})
		set(MTB_ASAN_MT ON)
		if (${MTB_ASAN_MT})
			add_compile_options(/MT)
		else()
			add_compile_options(/MD)
		endif()
	endif()
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rd_party")
add_subdirectory("${THIRD_PARTY_DIR}/SDL2")
add_subdirectory("${THIRD_PARTY_DIR}/imgui")
add_subdirectory("${THIRD_PARTY_DIR}/DirectXTK")
add_subdirectory("${THIRD_PARTY_DIR}/assimp")
add_subdirectory("${THIRD_PARTY_DIR}/cereal")

project(mtb)

set(SRC
	src/update_queue.hpp
	src/update_queue.cpp
	src/texture.hpp
	src/texture.cpp
	src/sh.hpp
	src/serialization.cpp
	src/scene_objects.hpp
	src/scene_objects.cpp
	src/rig.hpp
	src/rig.cpp
	src/rdr_queue.hpp
	src/rdr_queue.cpp
	src/rdr.hpp
	src/rdr.cpp
	src/path_helpers.hpp
	src/path_helpers.cpp
	src/model.hpp
	src/model.cpp
	src/math.hpp
	src/math.cpp
	src/main.cpp
	src/light.hpp
	src/light.cpp
	src/json_helpers.hpp
	src/json_helpers.cpp
	src/input.hpp
	src/input.cpp
	src/imgui_impl.hpp
	src/imgui_impl.cpp
	src/imgui.hpp
	src/hou_geo.hpp
	src/hou_geo.cpp
	src/gfx.hpp
	src/gfx.cpp
	src/common.hpp
	src/common.cpp
	src/camera.hpp
	src/camera.cpp
	src/assimp_loader.hpp
	src/assimp_loader.cpp
	src/anim.hpp
	src/anim.cpp
)
source_group("src" FILES ${SRC})

set(HLSL_INC
	hlsl/light.hlsli
	hlsl/material.hlsli
	hlsl/shader.hlsli
)
set(HLSL_VS
	hlsl/model_skin.vs.hlsl
	hlsl/model_solid.vs.hlsl
	hlsl/simple.vs.hlsl
)
set(HLSL_PS
	hlsl/model.hair.ps.hlsl
	hlsl/model.ps.hlsl
	hlsl/simple.ps.hlsl
)
set(HLSL ${HLSL_VS} ${HLSL_PS} ${HLSL_INC})
set_source_files_properties(${HLSL_VS} PROPERTIES VS_SHADER_TYPE Vertex VS_SHADER_MODEL 4.0 VS_SHADER_ENTRYPOINT "main")
set_source_files_properties(${HLSL_PS} PROPERTIES VS_SHADER_TYPE Pixel  VS_SHADER_MODEL 4.0 VS_SHADER_ENTRYPOINT "main")
source_group("hlsl" FILES ${HLSL})

add_executable(mtb ${SRC} ${HLSL})
target_link_libraries(mtb PRIVATE SDL2-static SDL2main imgui DirectXTK assimp cereal d3d11 d3dcompiler)


set (BIN_DIR "${CMAKE_SOURCE_DIR}/bin")
set_property(TARGET mtb PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR})
set_property(TARGET mtb PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR})
set_property(TARGET mtb PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BIN_DIR})

set_property(TARGET mtb PROPERTY COMPILE_FLAGS "/MP")


if (${MTB_CLANG_CL})
	target_compile_options(mtb PRIVATE -Wno-unknown-warning-option) # for cereal

	if (${MTB_ASAN})
		target_compile_options(mtb PRIVATE -fsanitize=address)
		target_compile_options(mtb PRIVATE /Od) # disable optimization for easier debugging in release mode
		# TODO: even if lld-link is used, it seems to ignore -fsanitize=address
		# See bug: https://bugs.llvm.org/show_bug.cgi?id=35343#c3
		# Also, see hint there to obtain list of required libraries.
		# set_target_properties(mtb PROPERTIES LINK_FLAGS "-fsanitize=address")
		set(LLVM_ROOT_PATH "e:/LLVM")
		set(LLVM_ASAN_LIB_PATH "${LLVM_ROOT_PATH}/lib/clang/6.0.0/lib/windows")
		if (${MTB_ASAN_MT})
			# use "clang-cl -v -fsanitize=address test.cpp" to obtain the list of required 
			target_link_libraries(mtb PRIVATE "${LLVM_ASAN_LIB_PATH}/clang_rt.asan-x86_64.lib")
			target_link_libraries(mtb PRIVATE "-wholearchive:${LLVM_ASAN_LIB_PATH}/clang_rt.asan-x86_64.lib")
			target_link_libraries(mtb PRIVATE "${LLVM_ASAN_LIB_PATH}/clang_rt.asan_cxx-x86_64.lib")
			target_link_libraries(mtb PRIVATE "-wholearchive:${LLVM_ASAN_LIB_PATH}/clang_rt.asan_cxx-x86_64.lib")
		else()
			target_link_libraries(mtb PRIVATE "${LLVM_ASAN_LIB_PATH}/clang_rt.asan_dynamic-x86_64.lib" )
			target_link_libraries(mtb PRIVATE "${LLVM_ASAN_LIB_PATH}/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib")
			target_link_libraries(mtb PRIVATE "-include:__asan_seh_interceptor")
			target_link_libraries(mtb PRIVATE "-wholearchive:${LLVM_ASAN_LIB_PATH}/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib")
		endif()
	endif()
endif()
