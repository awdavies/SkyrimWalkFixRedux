cmake_minimum_required(VERSION 3.22)

project(
	WalkFixRedux
	VERSION 1.0.0
	LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SKYRIM_POST_1_6_353 "Set for the big breaking change to 1.6.640" OFF)
option(SKYRIM_POST_1_6_640 "Set for the Dec 2023 breaking update" OFF)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

find_package(PolyHook_2 CONFIG REQUIRED)

macro(set_from_environment VARIABLE)
	if(NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif()
endmacro()

set_from_environment(Skyrim64Path)
if(NOT DEFINED Skyrim64Path)
	message(FATAL_ERROR "Skyrim64Path is not set")
endif()

option(COPY_OUTPUT "copy the output of build operations to the game directory" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

add_subdirectory(src)
include(cmake/packaging.cmake)
