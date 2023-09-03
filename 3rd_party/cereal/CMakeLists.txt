cmake_minimum_required(VERSION 3.27)

project(cereal)

add_library(cereal INTERFACE)
target_include_directories(cereal INTERFACE "cereal_git/include/")
# set_property(TARGET cereal PROPERTY FOLDER "3rd_party")
