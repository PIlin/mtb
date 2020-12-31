cmake_minimum_required(VERSION 3.3)

project(microprofile)

set(root ${CMAKE_CURRENT_SOURCE_DIR}/microprofile_git)
# set(hdrs ${root}/microprofile.h)

add_library(microprofile INTERFACE)
target_include_directories(microprofile INTERFACE "${root}")
# set_property(TARGET microprofile PROPERTY FOLDER "3rd_party")
