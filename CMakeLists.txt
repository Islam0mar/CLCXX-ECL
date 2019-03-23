cmake_minimum_required(VERSION 2.8.12)
project(CLCxx)

# Dependencies
# ============
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

find_program(ecl_EXECUTABLE ecl DOC "ecl executable")
execute_process(
  COMMAND "${ecl_EXECUTABLE}" -v
  OUTPUT_VARIABLE ecl_VERSION_STRING)
string(
  REGEX REPLACE ".*([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1"
  ecl_VERSION_STRING "${ecl_VERSION_STRING}")
message(STATUS "ecl vercion: ${ecl_VERSION_STRING}")

execute_process(
  COMMAND "${ecl_EXECUTABLE}-config" --libs
  OUTPUT_VARIABLE ecl_LIBRARY)
string(
  STRIP "${ecl_LIBRARY}" ecl_LIBRARY)
message(STATUS "libs: ${ecl_LIBRARY}")

execute_process(
  COMMAND "${ecl_EXECUTABLE}-config" --cflags
  OUTPUT_VARIABLE ecl_CFLAGS)
string(
  STRIP "${ecl_CFLAGS}" ecl_CFLAGS)
string(
  REGEX REPLACE " +" ";"
  ecl_CFLAGS "${ecl_CFLAGS}")
message(STATUS "cflags: ${ecl_CFLAGS}")


# set(ecl_INCLUDE_DIR "/usr/")
# set(ecl_LIBRARY_DIR "$ENV{HOME}/.local/lib")
# set(ecl_LIBRARY_DIR "$ENV{HOME}/.local/lib")

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${ecl_LIBRARY_DIR}")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_VERBOSE_MAKEFILE 1)

# includes
# ============
include(CheckCXXCompilerFlag)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
include(GNUInstallDirs)
include(InstallRequiredSystemLibraries)

# Compilation flags
# =================

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused-parameter -Wextra -Wreorder")
  CHECK_CXX_COMPILER_FLAG("-std=c++14" HAS_CPP14_FLAG)

  if (HAS_CPP14_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
  else()
    message(FATAL_ERROR "Unsupported compiler -- clcxx requires C++14 support!")
  endif()
endif()

if(WIN32 AND DEFINED ENV{MSYSTEM})
  if($ENV{MSYSTEM} STREQUAL "MINGW32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=pentium4 -static-libstdc++")
  endif()
endif()

set(CMAKE_INSTALL_LIBDIR "lib")

# set(CLCXX_BUILD_EXAMPLES ON CACHE BOOL "Build the CLCxx examples")
# set(CLCXX_BUILD_TESTS ON CACHE BOOL "Build the CLCxx tests")

# Source files
# ============

set(CLCXX_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CLCXX_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB HEADER_GLOB
  ${CLCXX_INCLUDE_DIR}/clcxx/*.hpp
  )

file(GLOB SOURCES_GLOB
  ${CLCXX_SOURCE_DIR}/*.cpp
  )

set(CLCXX_HEADERS
  ${HEADER_GLOB}
  )

set(CLCXX_SOURCES
  ${SOURCES_GLOB}
  )

# Versioning
# ==========

file(STRINGS "${CLCXX_INCLUDE_DIR}/clcxx/clcxx_config.hpp" clcxx_version_defines
  REGEX "#define CLCXX_VERSION_(MAJOR|MINOR|PATCH)")
foreach(ver ${clcxx_version_defines})
  if(ver MATCHES "#define CLCXX_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$")
    set(CLCXX_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
  endif()
endforeach()
set(${PROJECT_NAME}_VERSION
  ${CLCXX_VERSION_MAJOR}.${CLCXX_VERSION_MINOR}.${CLCXX_VERSION_PATCH})
message(STATUS "${PROJECT_NAME} version: v${${PROJECT_NAME}_VERSION}")

# Output
# ======

set(CLCXX_TARGET cxxwrap_lisp)
add_library(${CLCXX_TARGET} SHARED ${CLCXX_SOURCES} ${CLCXX_HEADERS})

target_include_directories(${CLCXX_TARGET} PUBLIC
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  $<BUILD_INTERFACE:${CLCXX_INCLUDE_DIR}>
  )

target_compile_options(${CLCXX_TARGET} PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:-std=c++14>" ${ecl_CFLAGS} "-Wall")

target_link_libraries(${CLCXX_TARGET} $<BUILD_INTERFACE:ecl> "-Wl,--no-undefined" "-Wl,--no-allow-shlib-undefined" "${ecl_LIBRARY}")
#link_directories(${ecl_LIBRARY_DIR} ${ecl_LIBRARY_DIR}/ecl-${ecl_VERSION_STRING})
set_target_properties(${CLCXX_TARGET} PROPERTIES
  PUBLIC_HEADER "${CLCXX_HEADERS}"
  COMPILE_DEFINITIONS "CLCXX_EXPORTS")
set_property(TARGET ${CLCXX_TARGET} PROPERTY VERSION ${${PROJECT_NAME}_VERSION})
set_property(TARGET ${CLCXX_TARGET} PROPERTY SOVERSION ${CLCXX_VERSION_MAJOR})
set_property(TARGET ${CLCXX_TARGET} PROPERTY
  INTERFACE_${CLCXX_TARGET}_MAJOR_VERSION ${CLCXX_VERSION_MAJOR})
set_property(TARGET ${CLCXX_TARGET} APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING ${CLCXX_TARGET}_MAJOR_VERSION
  )

generate_export_header(${CLCXX_TARGET})

set(CLCXX_NAME ${CLCXX_TARGET})
if(WIN32)
  if(MSVC)
    set(CLCXX_LINK_FILE ${CLCXX_NAME}${CMAKE_LINK_LIBRARY_SUFFIX})
    set(CLCXX_RUNTIME_FILE ${CLCXX_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
  else()
    set(CLCXX_LINK_FILE lib${CLCXX_NAME}.dll.a)
    set(CLCXX_RUNTIME_FILE lib${CLCXX_NAME}.dll)
  endif()
else()
  if(APPLE)
    target_link_libraries(${CLCXX_TARGET} "-framework CoreFoundation")
  endif()
  set(CLCXX_LINK_FILE lib${CLCXX_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
  set(CLCXX_RUNTIME_FILE lib${CLCXX_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
endif()

# Installation
# ============

# set(CLCXX_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for clcxxConfig.cmake")


# install(TARGETS ${CLCXX_TARGET} EXPORT CLCxxTargets
#         ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#         LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#         RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#         PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clcxx)

# configure_package_config_file(${PROJECT_NAME}Config.cmake.in
#                               "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
#                               INSTALL_DESTINATION ${CLCXX_CMAKECONFIG_INSTALL_DIR})
# write_basic_package_version_file(
#   "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
#   VERSION ${${PROJECT_NAME}_VERSION}
#   COMPATIBILITY AnyNewerVersion
# )

# # configure_file(Findecl.cmake ${CMAKE_CURRENT_BINARY_DIR}/Findecl.cmake COPYONLY)
# export(EXPORT CLCxxTargets
#   FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigExports.cmake
#   NAMESPACE ClCxx::
# )

# install(EXPORT CLCxxTargets
#   FILE ${PROJECT_NAME}ConfigExports.cmake
#   NAMESPACE CLCxx::
#   DESTINATION ${CLCXX_CMAKECONFIG_INSTALL_DIR}
# )

# install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
#               ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
#               ${CMAKE_CURRENT_SOURCE_DIR}/Findecl.cmake
#   DESTINATION ${CLCXX_CMAKECONFIG_INSTALL_DIR}
# )

# if(CLCXX_BUILD_EXAMPLES)
#   add_subdirectory(examples)
# endif()

# if(CLCXX_BUILD_TESTS)
#   enable_testing()
#   add_subdirectory(test)
# endif()


