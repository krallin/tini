cmake_minimum_required (VERSION 2.8.0)
project (tini C)

# Config
set (tini_VERSION_MAJOR 0)
set (tini_VERSION_MINOR 4)
set (tini_VERSION_PATCH 3)
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
	"${PROJECT_SOURCE_DIR}/tpl/README.md.in"
	"${PROJECT_SOURCE_DIR}/README.md"
	@ONLY
)

configure_file (
	"${PROJECT_SOURCE_DIR}/tpl/travis.yml.tpl"
	"${PROJECT_SOURCE_DIR}/.travis.yml"
	@ONLY
)

include_directories ("${PROJECT_BINARY_DIR}")

add_executable (tini src/tini.c)

# Installation
install (TARGETS tini DESTINATION bin)

# Packaging
include (InstallRequiredSystemLibraries)
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "A tiny but valid init process for containers")
set (CPACK_PACKAGE_VENDOR "Thomas Orozco")
set (CPACK_PACKAGE_CONTACT "thomas@orozco.fr")
set (CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set (CPACK_PACKAGE_VERSION_MAJOR "${tini_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${tini_VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${tini_VERSION_PATCH}")
set (CPACK_PACKAGE_EXECUTABLES "${CMAKE_PROJECT_NAME}")
set (CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set (CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${tini_VERSION_MAJOR}.${tini_VERSION_MINOR}.${tini_VERSION_PATCH}")
set (CPACK_PACKAGE_VERSION "${tini_VERSION_MAJOR}.${tini_VERSION_MINOR}.${tini_VERSION_PATCH}")

set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")  # TODO
set (CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.4)")

set (CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")

set (CPACK_GENERATOR "DEB" "RPM")

include (CPack)
