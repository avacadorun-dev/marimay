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
