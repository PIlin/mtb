cmake_minimum_required(VERSION 3.3)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "")
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/assimp_git")

target_include_directories(assimp PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/assimp_git/include>)
target_include_directories(assimp PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/assimp_git/include>)
set_property(TARGET assimp uninstall UpdateAssimpLibsDebugSymbolsAndDLLs zlibstatic PROPERTY FOLDER "3rd_party/assimp")
