# Compiler-wide C optimization without merging source ownership or changing the
# separately distributed native runtime archive profiles.
set(MMAY_COMPILER_IPO "AUTO" CACHE STRING "Compiler IPO: AUTO, ON, or OFF")
set_property(CACHE MMAY_COMPILER_IPO PROPERTY STRINGS AUTO ON OFF)
if(NOT MMAY_COMPILER_IPO MATCHES "^(AUTO|ON|OFF)$")
  message(FATAL_ERROR "MMAY_COMPILER_IPO must be AUTO, ON, or OFF")
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
  add_test(NAME mmay_compiler_ipo
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../tests/build/test_compiler_ipo.py"
      "${CMAKE_CURRENT_LIST_FILE}" "${CMAKE_COMMAND}")
  # Seven independently bounded 30-second configure probes plus teardown.
  set_tests_properties(mmay_compiler_ipo PROPERTIES TIMEOUT 240 LABELS "build;focused")
endif()

if(MMAY_COMPILER_IPO STREQUAL "OFF")
  return()
endif()
if(MMAY_COMPILER_IPO STREQUAL "AUTO" AND NOT CMAKE_CONFIGURATION_TYPES
    AND NOT CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
  return()
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT mmay_ipo_supported OUTPUT mmay_ipo_error LANGUAGES C)
if(NOT mmay_ipo_supported)
  if(MMAY_COMPILER_IPO STREQUAL "ON")
    message(FATAL_ERROR "Requested compiler IPO is unavailable: ${mmay_ipo_error}")
  endif()
  message(STATUS "Compiler IPO unavailable; using ordinary C17 compilation: ${mmay_ipo_error}")
  return()
endif()

function(mmay_apply_compiler_ipo directory)
  get_property(mmay_ipo_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(mmay_ipo_target IN LISTS mmay_ipo_targets)
    get_target_property(mmay_ipo_links "${mmay_ipo_target}" LINK_LIBRARIES)
    if(mmay_ipo_target STREQUAL "mmay_bootstrap" OR mmay_ipo_target STREQUAL "mmay_sljit"
        OR "mmay_bootstrap" IN_LIST mmay_ipo_links)
      if(MMAY_COMPILER_IPO STREQUAL "ON")
        set_property(TARGET "${mmay_ipo_target}" PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
      else()
        foreach(mmay_ipo_config RELEASE RELWITHDEBINFO MINSIZEREL)
          set_property(TARGET "${mmay_ipo_target}"
            PROPERTY "INTERPROCEDURAL_OPTIMIZATION_${mmay_ipo_config}" TRUE)
        endforeach()
      endif()
    endif()
  endforeach()
  get_property(mmay_ipo_children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(mmay_ipo_child IN LISTS mmay_ipo_children)
    mmay_apply_compiler_ipo("${mmay_ipo_child}")
  endforeach()
endfunction()
mmay_apply_compiler_ipo("${PROJECT_SOURCE_DIR}")
message(STATUS "Compiler IPO enabled (${MMAY_COMPILER_IPO}); native runtime profiles remain independent")
