cmake_minimum_required (VERSION 2.8.0)
project (tini C)

# Config
set (tini_VERSION_MAJOR 0)
set (tini_VERSION_MINOR 3)
set (tini_VERSION_PATCH 4)
execute_process (COMMAND git log -n 1 --date=local --pretty=format:"%h" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE tini_VERSION_GIT)

# Flags
add_definitions (-D_FORTIFY_SOURCE=2)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wextra -Wall -pedantic -O2 -fstack-protector --param=ssp-buffer-size=4 -Wformat -Werror=format-security")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bsymbolic-functions -Wl,-z,relro -Wl,-s")

# Build

configure_file (
	"${PROJECT_SOURCE_DIR}/src/tiniConfig.h.in"
	"${PROJECT_BINARY_DIR}/tiniConfig.h"
	@ONLY
)

configure_file (
	"${PROJECT_SOURCE_DIR}/README.md.in"
	"${PROJECT_SOURCE_DIR}/README.md"
	@ONLY
)

include_directories ("${PROJECT_BINARY_DIR}")

add_executable (tini src/tini.c)

# Installation
install (TARGETS tini DESTINATION bin)
