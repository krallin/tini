cmake_minimum_required (VERSION 2.8.0)
project (tini C)

# Config
set (tini_VERSION_MAJOR 0)
set (tini_VERSION_MINOR 13)
set (tini_VERSION_PATCH 0)

# Build options
option(MINIMAL "Disable argument parsing and verbose output" OFF)

if(MINIMAL)
	add_definitions(-DTINI_MINIMAL=1)
endif()

# Extract git version and dirty-ness
execute_process (
  COMMAND git --git-dir "${PROJECT_SOURCE_DIR}/.git" --work-tree "${PROJECT_SOURCE_DIR}" log -n 1 --date=local --pretty=format:%h
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  RESULT_VARIABLE git_version_check_ret
  OUTPUT_VARIABLE tini_VERSION_GIT
)

execute_process(
  COMMAND git --git-dir "${PROJECT_SOURCE_DIR}/.git" --work-tree "${PROJECT_SOURCE_DIR}" status --porcelain --untracked-files=no
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  OUTPUT_VARIABLE git_dirty_check_out
)

if("${git_version_check_ret}" EQUAL 0)
  set(tini_VERSION_GIT " - git.${tini_VERSION_GIT}")
  if(NOT "${git_dirty_check_out}" STREQUAL "")
    set(tini_VERSION_GIT "${tini_VERSION_GIT}-dirty")
  endif()
else()
  set(tini_VERSION_GIT "")
endif()

# Flags
include(CheckCSourceCompiles)

check_c_source_compiles("
#ifndef _FORTIFY_SOURCE
#error \"Not defined: _FORTIFY_SOURCE\"
#endif
int main(void)                                                                                                              {
  return 0;
}
" HAS_BUILTIN_FORTIFY)

# Flags
if(NOT HAS_BUILTIN_FORTIFY)
	add_definitions(-D_FORTIFY_SOURCE=2)
endif()

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Werror -Wextra -Wall -pedantic-errors -O2 -fstack-protector --param=ssp-buffer-size=4 -Wformat")
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
	"${PROJECT_SOURCE_DIR}/tpl/VERSION.in"
	"${PROJECT_BINARY_DIR}/VERSION"
	@ONLY
)


include_directories ("${PROJECT_BINARY_DIR}")

add_executable (tini src/tini.c)

add_executable (tini-static src/tini.c)
set_target_properties (tini-static PROPERTIES LINK_FLAGS "-Wl,--no-export-dynamic -static")

# Installation
install (TARGETS tini DESTINATION bin)
install (TARGETS tini-static DESTINATION bin)

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
