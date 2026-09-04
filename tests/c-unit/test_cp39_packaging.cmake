cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS
    MMAY_TEST_BINARY_DIR MMAY_TEST_CONFIG MMAY_TEST_SOURCE_DIR
    MMAY_TEST_VERSION MMAY_TEST_HOST MMAY_TEST_ARCHIVE_FORMAT
    MMAY_TEST_SOURCE_DATE_EPOCH MMAY_TEST_EXECUTABLE_SUFFIX)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "missing CP39 packaging test input: ${required}")
  endif()
endforeach()

set(mmay_root "${MMAY_TEST_BINARY_DIR}/cp39-packaging")
set(mmay_distribution "${MMAY_TEST_BINARY_DIR}/distribution")
set(mmay_staged "${mmay_distribution}/stage")
set(mmay_replay "${mmay_root}/replay")
set(mmay_packager "${MMAY_TEST_SOURCE_DIR}/tools/package-distribution")
set(host_archive
  "marimay-${MMAY_TEST_VERSION}-${MMAY_TEST_HOST}.${MMAY_TEST_ARCHIVE_FORMAT}")
set(source_archive "marimay-${MMAY_TEST_VERSION}-source.tar.gz")
file(REMOVE_RECURSE "${mmay_root}")
file(MAKE_DIRECTORY "${mmay_root}")

# Exercise the public package target once. Its verified output remains in the
# canonical distribution directory for the CI artifact upload step.
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${MMAY_TEST_BINARY_DIR}"
    --config "${MMAY_TEST_CONFIG}" --target mmay_package --parallel 2
  RESULT_VARIABLE target_result
  OUTPUT_VARIABLE target_output
  ERROR_VARIABLE target_error
  ENCODING UTF-8
)
if(NOT target_result EQUAL 0)
  message(FATAL_ERROR
    "CP39 package target failed\n${target_output}\n${target_error}")
endif()

foreach(archive IN ITEMS "${host_archive}" "${source_archive}")
  foreach(suffix IN ITEMS "" ".sha256")
    if(NOT EXISTS "${mmay_distribution}/${archive}${suffix}")
      message(FATAL_ERROR
        "CP39 package target omitted ${archive}${suffix}")
    endif()
  endforeach()
endforeach()

foreach(archive IN ITEMS "${host_archive}" "${source_archive}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar tf
      "${mmay_distribution}/${archive}"
    RESULT_VARIABLE archive_list_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE archive_list_error
    ENCODING UTF-8
  )
  if(NOT archive_list_result EQUAL 0)
    message(FATAL_ERROR
      "cannot list CP39 archive ${archive}: ${archive_list_error}")
  endif()
  if(archive_members MATCHES "(^|\n)([^\n]*/)?[.]mmay(/|\n)")
    message(FATAL_ERROR "archive contains tool-owned .mmay state: ${archive}")
  endif()
endforeach()

# Replay the packager from the exact staged install. Comparing independent
# invocations keeps reproducibility coverage while avoiding two complete
# packaging passes in every Debug and sanitizer profile.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${mmay_packager}"
    --install-root "${mmay_staged}"
    --source-root "${MMAY_TEST_SOURCE_DIR}"
    --output-directory "${mmay_replay}"
    --version "${MMAY_TEST_VERSION}"
    --host "${MMAY_TEST_HOST}"
    --epoch "${MMAY_TEST_SOURCE_DATE_EPOCH}"
    --host-format "${MMAY_TEST_ARCHIVE_FORMAT}"
  RESULT_VARIABLE replay_result
  OUTPUT_VARIABLE replay_output
  ERROR_VARIABLE replay_error
  ENCODING UTF-8
)
if(NOT replay_result EQUAL 0)
  message(FATAL_ERROR
    "CP39 package replay failed\n${replay_output}\n${replay_error}")
endif()

foreach(archive IN ITEMS "${host_archive}" "${source_archive}")
  file(SHA256 "${mmay_distribution}/${archive}" target_hash)
  file(SHA256 "${mmay_replay}/${archive}" replay_hash)
  if(NOT target_hash STREQUAL replay_hash)
    message(FATAL_ERROR "archive is not reproducible: ${archive}")
  endif()
  file(READ "${mmay_distribution}/${archive}.sha256" target_checksum)
  file(READ "${mmay_replay}/${archive}.sha256" replay_checksum)
  if(NOT target_checksum STREQUAL "${target_hash}  ${archive}\n"
      OR NOT replay_checksum STREQUAL target_checksum)
    message(FATAL_ERROR "archive checksum sibling mismatch: ${archive}")
  endif()
endforeach()

set(mmay_source_extract "${mmay_root}/source archive")
file(MAKE_DIRECTORY "${mmay_source_extract}")
file(ARCHIVE_EXTRACT
  INPUT "${mmay_distribution}/${source_archive}"
  DESTINATION "${mmay_source_extract}")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${mmay_source_extract}/marimay-${MMAY_TEST_VERSION}-source"
    -B "${mmay_source_extract}/configured"
    -DBUILD_TESTING=OFF
  RESULT_VARIABLE source_configure_result
  OUTPUT_VARIABLE source_configure_output
  ERROR_VARIABLE source_configure_error
  ENCODING UTF-8
)
if(NOT source_configure_result EQUAL 0)
  message(FATAL_ERROR
    "offline source archive does not configure\n${source_configure_output}\n${source_configure_error}")
endif()

# The portable source packager excludes tracked tool-state fixtures by path,
# rather than relying on the main repository to happen not to contain one.
set(mmay_synthetic_source "${mmay_root}/synthetic source")
set(mmay_synthetic_install "${mmay_root}/synthetic install")
set(mmay_synthetic_output "${mmay_root}/synthetic output")
file(MAKE_DIRECTORY
  "${mmay_synthetic_source}/.mmay"
  "${mmay_synthetic_install}/bin")
file(WRITE "${mmay_synthetic_source}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.25)\nproject(cp39 NONE)\n")
file(WRITE "${mmay_synthetic_source}/.mmay/private.my" "private\n")
file(WRITE "${mmay_synthetic_install}/bin/mmay" "fixture\n")
find_program(mmay_git git REQUIRED)
execute_process(
  COMMAND "${mmay_git}" init --quiet
  WORKING_DIRECTORY "${mmay_synthetic_source}"
  RESULT_VARIABLE synthetic_git_result
)
if(NOT synthetic_git_result EQUAL 0)
  message(FATAL_ERROR "cannot initialize the synthetic source repository")
endif()
execute_process(
  COMMAND "${mmay_git}" add -f CMakeLists.txt .mmay/private.my
  WORKING_DIRECTORY "${mmay_synthetic_source}"
  RESULT_VARIABLE synthetic_add_result
)
if(NOT synthetic_add_result EQUAL 0)
  message(FATAL_ERROR "cannot populate the synthetic source repository")
endif()
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${mmay_packager}"
    --install-root "${mmay_synthetic_install}"
    --source-root "${mmay_synthetic_source}"
    --output-directory "${mmay_synthetic_output}"
    --version "${MMAY_TEST_VERSION}"
    --host "${MMAY_TEST_HOST}"
    --epoch "${MMAY_TEST_SOURCE_DATE_EPOCH}"
    --host-format "${MMAY_TEST_ARCHIVE_FORMAT}"
  RESULT_VARIABLE synthetic_package_result
  OUTPUT_VARIABLE synthetic_package_output
  ERROR_VARIABLE synthetic_package_error
  ENCODING UTF-8
)
if(NOT synthetic_package_result EQUAL 0)
  message(FATAL_ERROR
    "synthetic source packaging failed\n${synthetic_package_output}\n${synthetic_package_error}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar tf
    "${mmay_synthetic_output}/${source_archive}"
  RESULT_VARIABLE synthetic_list_result
  OUTPUT_VARIABLE synthetic_members
  ERROR_VARIABLE synthetic_list_error
  ENCODING UTF-8
)
if(NOT synthetic_list_result EQUAL 0
    OR synthetic_members MATCHES "(^|\n)([^\n]*/)?[.]mmay(/|\n)"
    OR NOT synthetic_members MATCHES "CMakeLists[.]txt")
  message(FATAL_ERROR
    "source archive state exclusion failed: ${synthetic_members}/${synthetic_list_error}")
endif()

execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${mmay_packager}"
    --install-root "${mmay_synthetic_install}"
    --source-root "${mmay_synthetic_source}"
    --output-directory "${mmay_root}/unsafe output"
    --version "../escape"
    --host "${MMAY_TEST_HOST}"
    --epoch "${MMAY_TEST_SOURCE_DATE_EPOCH}"
    --host-format "${MMAY_TEST_ARCHIVE_FORMAT}"
  RESULT_VARIABLE unsafe_name_result
  OUTPUT_VARIABLE unsafe_name_output
  ERROR_VARIABLE unsafe_name_error
  ENCODING UTF-8
)
if(unsafe_name_result EQUAL 0
    OR EXISTS "${mmay_root}/marimay-escape-source.tar.gz")
  message(FATAL_ERROR
    "packager accepted an unsafe archive component: ${unsafe_name_output}/${unsafe_name_error}")
endif()

if(UNIX)
  set(mmay_linked_output "${mmay_root}/linked output")
  file(CREATE_LINK "${mmay_synthetic_source}/CMakeLists.txt"
    "${mmay_synthetic_source}/linked.txt" SYMBOLIC RESULT source_link_result)
  if(NOT source_link_result STREQUAL "0")
    message(FATAL_ERROR "cannot create the tracked source-link fixture")
  endif()
  execute_process(
    COMMAND "${mmay_git}" add linked.txt
    WORKING_DIRECTORY "${mmay_synthetic_source}"
    RESULT_VARIABLE source_link_add_result
  )
  if(NOT source_link_add_result EQUAL 0)
    message(FATAL_ERROR "cannot track the source-link fixture")
  endif()
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${mmay_packager}"
      --install-root "${mmay_synthetic_install}"
      --source-root "${mmay_synthetic_source}"
      --output-directory "${mmay_linked_output}"
      --version "${MMAY_TEST_VERSION}"
      --host "${MMAY_TEST_HOST}"
      --epoch "${MMAY_TEST_SOURCE_DATE_EPOCH}"
      --host-format "${MMAY_TEST_ARCHIVE_FORMAT}"
    RESULT_VARIABLE linked_source_result
    OUTPUT_VARIABLE linked_source_output
    ERROR_VARIABLE linked_source_error
    ENCODING UTF-8
  )
  if(linked_source_result EQUAL 0 OR EXISTS "${mmay_linked_output}")
    message(FATAL_ERROR
      "packager accepted a tracked source link: ${linked_source_output}/${linked_source_error}")
  endif()
endif()

set(mmay_extracted "${mmay_root}/archive relocation")
file(MAKE_DIRECTORY "${mmay_extracted}")
file(ARCHIVE_EXTRACT
  INPUT "${mmay_distribution}/${host_archive}"
  DESTINATION "${mmay_extracted}")
execute_process(
  COMMAND
    "${mmay_extracted}/bin/mmay${MMAY_TEST_EXECUTABLE_SUFFIX}" doctor --json
  RESULT_VARIABLE doctor_result
  OUTPUT_VARIABLE doctor_output
  ERROR_VARIABLE doctor_error
  ENCODING UTF-8
)
if(NOT doctor_result EQUAL 0 OR NOT doctor_error STREQUAL ""
    OR NOT doctor_output MATCHES "\"ok\":true")
  message(FATAL_ERROR
    "extracted host archive is unhealthy: ${doctor_result}/${doctor_output}/${doctor_error}")
endif()
