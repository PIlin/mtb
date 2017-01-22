cmake_minimum_required(VERSION 3.2)

set(SDL_SHARED OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rd_party")
add_subdirectory("${THIRD_PARTY_DIR}/SDL2")
add_subdirectory("${THIRD_PARTY_DIR}/imgui")
add_subdirectory("${THIRD_PARTY_DIR}/DirectXTK")
add_subdirectory("${THIRD_PARTY_DIR}/assimp")
add_subdirectory("${THIRD_PARTY_DIR}/cereal")

project(mtb)

set(SRC
	src/texture.hpp
	src/texture.cpp
	src/sh.hpp
	src/serialization.cpp
	src/rig.hpp
	src/rig.cpp
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

include_directories(${SDL2_SOURCE_DIR}/include)

add_executable(mtb ${SRC} ${HLSL})
target_link_libraries(mtb SDL2-static SDL2main imgui DirectXTK assimp cereal d3d11 d3dcompiler)


set (BIN_DIR "${CMAKE_SOURCE_DIR}/bin")
set_property(TARGET mtb PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR})
set_property(TARGET mtb PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR})
set_property(TARGET mtb PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BIN_DIR})

