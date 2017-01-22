cmake_minimum_required(VERSION 3.3)

set(BUILD_SHARED_LIBS OFF)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_INSTALL_PDB OFF)
set(ASSIMP_BUILD_ZLIB ON)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/assimp_git")

target_include_directories(assimp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/assimp_git/include")
set_property(TARGET assimp uninstall UpdateAssimpLibsDebugSymbolsAndDLLs zlibstatic PROPERTY FOLDER "3rd_party/assimp")
