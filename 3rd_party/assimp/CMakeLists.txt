cmake_minimum_required(VERSION 3.3)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "")
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "")

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "")

if (${CMAKE_GENERATOR_TOOLSET} MATCHES "LLVM-vs")
	add_compile_options(-Wno-pragma-pack)
	add_compile_options(-Wno-delete-non-virtual-dtor)
	add_compile_options(-Wno-invalid-offsetof)
	add_compile_options(-Wno-unknown-warning-option)
	add_compile_options(-Wno-unused-value)
	add_compile_options(-Wno-unused-variable)
	add_compile_options(-Wno-uninitialized)
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/assimp_git")

target_include_directories(assimp PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/assimp_git/include>)
target_include_directories(assimp PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/assimp_git/include>)
set_property(TARGET assimp uninstall UpdateAssimpLibsDebugSymbolsAndDLLs zlibstatic IrrXML zlib PROPERTY FOLDER "3rd_party/assimp")
