cmake_minimum_required(VERSION 3.3)

set(BUILD_TESTING OFF CACHE BOOL "")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/entt_git")
set_target_properties(aob PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(aob PROPERTIES FOLDER "3rd_party/entt")
