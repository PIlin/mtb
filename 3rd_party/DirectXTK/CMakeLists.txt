cmake_minimum_required(VERSION 3.27)

project(DirectXTK)

set(hdrs 
	Inc/WICTextureLoader.h
	Inc/DirectXHelpers.h
	Inc/DDSTextureLoader.h
)

set(srcs 
	Src/pch.h
	Src/pch.cpp
	Src/dds.h
	Src/WICTextureLoader.cpp
	Src/PlatformHelpers.h
	Src/DDSTextureLoader.cpp
)

if (${CMAKE_GENERATOR_TOOLSET} MATCHES "LLVM-vs")
	add_compile_options(-Wno-missing-braces)
	add_compile_options(-Wno-switch)
endif()


include_directories(Inc)

add_library(DirectXTK STATIC ${srcs} ${hdrs})
target_include_directories(DirectXTK INTERFACE "Inc")
set_property(TARGET DirectXTK PROPERTY FOLDER "3rd_party")