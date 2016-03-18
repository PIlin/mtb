cmake_minimum_required(VERSION 3.3)

project(assimp)

# add_library(assimp INTERFACE)
# target_include_directories(assimp INTERFACE "include/")


add_library(assimp IMPORTED SHARED GLOBAL)
set_property(TARGET assimp PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include/")
set_property(TARGET assimp PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/x64/assimp-vc100-mt.dll")
set_property(TARGET assimp PROPERTY IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/x64/assimp-vc100-mt.lib")
