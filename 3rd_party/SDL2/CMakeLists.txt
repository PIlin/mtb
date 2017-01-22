cmake_minimum_required(VERSION 3.3)

set(SDL_SHARED OFF)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/sdl2_git")

target_include_directories(SDL2-static INTERFACE "${SDL2_SOURCE_DIR}/include")
set_property(TARGET SDL2-static SDL2main PROPERTY FOLDER "3rd_party/sdl2")
