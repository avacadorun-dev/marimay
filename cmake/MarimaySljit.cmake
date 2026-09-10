# The portable bootstrap remains dependency-free unless explicitly enabled.
option(MMAY_ENABLE_SLJIT "Enable the experimental verified VM machine-code tier" OFF)
set(MMAY_SLJIT_SOURCE_DIR "" CACHE PATH "Local SLJIT sources matching cmake/sljit-source.json")

install(FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sljit-source.json"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sljit-arm64-minimum.patch"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/mmay/${MMAY_COMPILER_VERSION}/cmake"
)

if(NOT MMAY_ENABLE_SLJIT)
  return()
endif()
if(NOT IS_DIRECTORY "${MMAY_SLJIT_SOURCE_DIR}")
  message(FATAL_ERROR "MMAY_ENABLE_SLJIT requires a local MMAY_SLJIT_SOURCE_DIR; no dependency is downloaded during build")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(mmay_sljit_prepared "${CMAKE_CURRENT_BINARY_DIR}/mmay-sljit-inputs")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare-sljit.py"
    --source "${MMAY_SLJIT_SOURCE_DIR}"
    --output "${mmay_sljit_prepared}"
    --manifest "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sljit-source.json"
    --config "${CMAKE_CURRENT_SOURCE_DIR}/bootstrap/src/bytecode/sljitConfigPre.h"
    --patch "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sljit-arm64-minimum.patch"
  RESULT_VARIABLE mmay_sljit_prepare_result
)
if(NOT mmay_sljit_prepare_result EQUAL 0)
  message(FATAL_ERROR "Pinned SLJIT input preparation failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare-sljit.py"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sljit-source.json"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sljit-arm64-minimum.patch"
  "${CMAKE_CURRENT_SOURCE_DIR}/bootstrap/src/bytecode/sljitConfigPre.h"
)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sljit-source.json" mmay_sljit_manifest)
string(JSON mmay_sljit_file_count LENGTH "${mmay_sljit_manifest}" files_sha256)
math(EXPR mmay_sljit_last_file "${mmay_sljit_file_count} - 1")
foreach(mmay_sljit_index RANGE 0 ${mmay_sljit_last_file})
  string(JSON mmay_sljit_file MEMBER "${mmay_sljit_manifest}" files_sha256 ${mmay_sljit_index})
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MMAY_SLJIT_SOURCE_DIR}/${mmay_sljit_file}")
endforeach()

add_library(mmay_sljit STATIC "${mmay_sljit_prepared}/sljit_src/sljitLir.c")
target_compile_definitions(mmay_sljit PRIVATE SLJIT_HAVE_CONFIG_PRE=1)
target_include_directories(mmay_sljit PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/bootstrap/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/bootstrap/src/bytecode"
)
# Keep third-party warnings separate, while retaining the same sanitizer gates.
mmay_target_sanitizers(mmay_sljit)

target_compile_definitions(mmay_bootstrap PRIVATE MMAY_HAVE_SLJIT=1)
set_source_files_properties(bootstrap/src/bytecode/vm_jit_sljit.c PROPERTIES
  COMPILE_DEFINITIONS SLJIT_HAVE_CONFIG_PRE=1
  INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/bootstrap/src/bytecode")
target_include_directories(mmay_bootstrap SYSTEM PRIVATE "${mmay_sljit_prepared}/sljit_src")
target_link_libraries(mmay_bootstrap PUBLIC mmay_sljit)
install(FILES "${mmay_sljit_prepared}/LICENSE"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/mmay/${MMAY_COMPILER_VERSION}/licenses"
  RENAME SLJIT-BSD-2-Clause.txt)
