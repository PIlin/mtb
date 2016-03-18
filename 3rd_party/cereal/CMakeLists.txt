cmake_minimum_required(VERSION 3.3)

project(cereal)

add_library(cereal INTERFACE)
target_include_directories(cereal INTERFACE "cereal_git/include/")

