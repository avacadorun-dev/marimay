cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS
    MMAY_TEST_BINARY_DIR MMAY_TEST_CONFIG MMAY_TEST_SOURCE_DIR
    MMAY_TEST_VERSION MMAY_TEST_EXECUTABLE_SUFFIX)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "missing CP39 test input: ${required}")
  endif()
endforeach()

set(mmay_root "${MMAY_TEST_BINARY_DIR}/cp39-distribution")
set(mmay_staged "${mmay_root}/staged")
set(mmay_prefix "${mmay_root}/installed path ü")
set(mmay_relocated "${mmay_root}/relocated path ü")
set(mmay_project "${mmay_root}/alpha smoke ü")
set(mmay_lifecycle
  "${MMAY_TEST_SOURCE_DIR}/cmake/MarimayDistributionLifecycle.cmake")
set(mmay_program "${mmay_prefix}/bin/mmay${MMAY_TEST_EXECUTABLE_SUFFIX}")
file(REMOVE_RECURSE "${mmay_root}")
file(MAKE_DIRECTORY "${mmay_root}")

function(mmay_expect_exit expected description)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    ENCODING UTF-8
  )
  if(NOT result EQUAL expected)
    message(FATAL_ERROR
      "${description}: expected ${expected}, got ${result}\nstdout=${output}\nstderr=${error}")
  endif()
  set(MMAY_LAST_OUTPUT "${output}" PARENT_SCOPE)
  set(MMAY_LAST_ERROR "${error}" PARENT_SCOPE)
endfunction()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${MMAY_TEST_BINARY_DIR}"
    --config "${MMAY_TEST_CONFIG}" --prefix "${mmay_staged}"
  RESULT_VARIABLE install_result
  OUTPUT_VARIABLE install_output
  ERROR_VARIABLE install_error
  ENCODING UTF-8
)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR
    "CP39 staging install failed\n${install_output}\n${install_error}")
endif()

mmay_expect_exit(0 "clean lifecycle install"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=install"
  "-DMMAY_DISTRIBUTION_SOURCE=${mmay_staged}"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_prefix}"
  -P "${mmay_lifecycle}")

# A damaged extracted archive must be rejected before either a staging sibling
# or a partially published prefix can appear. Reuse one private copy so these
# two integrity cases do not duplicate the complete distribution setup.
set(mmay_invalid_source "${mmay_root}/invalid extracted tree")
set(mmay_invalid_prefix "${mmay_root}/invalid install")
file(COPY "${mmay_staged}/" DESTINATION "${mmay_invalid_source}")
file(APPEND "${mmay_invalid_source}/bin/mmay${MMAY_TEST_EXECUTABLE_SUFFIX}"
  "corrupt\n")
mmay_expect_exit(1 "checksum-mismatched install source"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=install"
  "-DMMAY_DISTRIBUTION_SOURCE=${mmay_invalid_source}"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_invalid_prefix}"
  -P "${mmay_lifecycle}")
if(EXISTS "${mmay_invalid_prefix}"
    OR EXISTS "${mmay_invalid_prefix}.mmay-stage")
  message(FATAL_ERROR "checksum failure published partial install state")
endif()
file(COPY_FILE
  "${mmay_staged}/bin/mmay${MMAY_TEST_EXECUTABLE_SUFFIX}"
  "${mmay_invalid_source}/bin/mmay${MMAY_TEST_EXECUTABLE_SUFFIX}")
file(REMOVE "${mmay_invalid_source}/include/mmay/runtime/core.h")
mmay_expect_exit(1 "incomplete install source"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=install"
  "-DMMAY_DISTRIBUTION_SOURCE=${mmay_invalid_source}"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_invalid_prefix}"
  -P "${mmay_lifecycle}")
if(EXISTS "${mmay_invalid_prefix}"
    OR EXISTS "${mmay_invalid_prefix}.mmay-stage")
  message(FATAL_ERROR "incomplete source published partial install state")
endif()
file(COPY_FILE
  "${mmay_staged}/include/mmay/runtime/core.h"
  "${mmay_invalid_source}/include/mmay/runtime/core.h")
file(WRITE "${mmay_invalid_source}/unlisted-payload" "unowned\n")
mmay_expect_exit(1 "unlisted install payload"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=install"
  "-DMMAY_DISTRIBUTION_SOURCE=${mmay_invalid_source}"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_invalid_prefix}"
  -P "${mmay_lifecycle}")
if(EXISTS "${mmay_invalid_prefix}"
    OR EXISTS "${mmay_invalid_prefix}.mmay-stage")
  message(FATAL_ERROR "unlisted payload published partial install state")
endif()
file(REMOVE "${mmay_invalid_source}/unlisted-payload")
set(mmay_invalid_manifest
  "${mmay_invalid_source}/share/mmay/${MMAY_TEST_VERSION}/checksums.sha256")
file(STRINGS "${mmay_invalid_manifest}" first_manifest_line LIMIT_COUNT 1)
file(APPEND "${mmay_invalid_manifest}" "${first_manifest_line}\n")
mmay_expect_exit(1 "duplicate install manifest path"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=install"
  "-DMMAY_DISTRIBUTION_SOURCE=${mmay_invalid_source}"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_invalid_prefix}"
  -P "${mmay_lifecycle}")
if(EXISTS "${mmay_invalid_prefix}"
    OR EXISTS "${mmay_invalid_prefix}.mmay-stage")
  message(FATAL_ERROR "duplicate manifest path published partial install state")
endif()
string(REGEX MATCH "^([0-9a-f]+)  (.+)$" unused "${first_manifest_line}")
file(WRITE "${mmay_invalid_manifest}"
  "${CMAKE_MATCH_1}  ./${CMAKE_MATCH_2}\n")
mmay_expect_exit(1 "non-normalized install manifest path"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=install"
  "-DMMAY_DISTRIBUTION_SOURCE=${mmay_invalid_source}"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_invalid_prefix}"
  -P "${mmay_lifecycle}")
if(EXISTS "${mmay_invalid_prefix}"
    OR EXISTS "${mmay_invalid_prefix}.mmay-stage")
  message(FATAL_ERROR "unsafe manifest path published partial install state")
endif()
file(COPY_FILE
  "${mmay_staged}/share/mmay/${MMAY_TEST_VERSION}/checksums.sha256"
  "${mmay_invalid_manifest}")
if(UNIX)
  file(REMOVE_RECURSE "${mmay_invalid_source}/include")
  file(CREATE_LINK "${mmay_staged}/include"
    "${mmay_invalid_source}/include" SYMBOLIC RESULT link_result)
  if(NOT link_result STREQUAL "0")
    message(FATAL_ERROR "cannot create CP39 distribution symlink fixture")
  endif()
  mmay_expect_exit(1 "linked install payload"
    "${CMAKE_COMMAND}"
    "-DMMAY_DISTRIBUTION_ACTION=install"
    "-DMMAY_DISTRIBUTION_SOURCE=${mmay_invalid_source}"
    "-DMMAY_DISTRIBUTION_PREFIX=${mmay_invalid_prefix}"
    -P "${mmay_lifecycle}")
  if(EXISTS "${mmay_invalid_prefix}"
      OR EXISTS "${mmay_invalid_prefix}.mmay-stage")
    message(FATAL_ERROR "linked payload published partial install state")
  endif()
endif()
file(REMOVE_RECURSE "${mmay_invalid_source}")

# POSIX permission bits are part of the currently claimed Linux lifecycle.
# Action containers execute as root, so drop to the standard unprivileged
# account there; ordinary developer runs exercise the same command directly.
if(UNIX)
  set(mmay_readonly_parent "${mmay_root}/read-only parent")
  set(mmay_readonly_prefix "${mmay_readonly_parent}/install")
  file(MAKE_DIRECTORY "${mmay_readonly_parent}")
  file(CHMOD "${mmay_readonly_parent}" PERMISSIONS
    OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  execute_process(
    COMMAND id -u
    RESULT_VARIABLE id_result
    OUTPUT_VARIABLE user_id
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT id_result EQUAL 0)
    message(FATAL_ERROR "cannot determine identity for read-only prefix test")
  endif()
  set(mmay_readonly_command "${CMAKE_COMMAND}")
  if(user_id STREQUAL "0")
    find_program(mmay_runuser runuser REQUIRED)
    set(mmay_readonly_command "${mmay_runuser}" -u nobody --
      "${CMAKE_COMMAND}")
  endif()
  mmay_expect_exit(1 "read-only install destination"
    ${mmay_readonly_command}
    "-DMMAY_DISTRIBUTION_ACTION=install"
    "-DMMAY_DISTRIBUTION_SOURCE=${mmay_staged}"
    "-DMMAY_DISTRIBUTION_PREFIX=${mmay_readonly_prefix}"
    -P "${mmay_lifecycle}")
  if(EXISTS "${mmay_readonly_prefix}"
      OR EXISTS "${mmay_readonly_prefix}.mmay-stage")
    message(FATAL_ERROR "read-only failure published partial install state")
  endif()
  file(CHMOD "${mmay_readonly_parent}" PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

foreach(required_file IN ITEMS
    "bin/mmay${MMAY_TEST_EXECUTABLE_SUFFIX}"
    "share/mmay/${MMAY_TEST_VERSION}/checksums.sha256"
    "share/mmay/${MMAY_TEST_VERSION}/provenance.json"
    "share/mmay/${MMAY_TEST_VERSION}/sbom.spdx.json"
    "share/mmay/${MMAY_TEST_VERSION}/std/io/io.my"
    "share/mmay/${MMAY_TEST_VERSION}/std/cli/cli.my"
    "share/mmay/${MMAY_TEST_VERSION}/std/json/json.my"
    "share/doc/mmay/${MMAY_TEST_VERSION}/README.md"
    "share/doc/mmay/${MMAY_TEST_VERSION}/CMakeLists.txt"
    "share/doc/mmay/${MMAY_TEST_VERSION}/bootstrap/main.c"
    "share/doc/mmay/${MMAY_TEST_VERSION}/bootstrap/src/generated/opcode_data.inc"
    "share/doc/mmay/${MMAY_TEST_VERSION}/bootstrap/tables/opcodes.tsv"
    "share/doc/mmay/${MMAY_TEST_VERSION}/std/io/io.my")
  if(NOT EXISTS "${mmay_prefix}/${required_file}")
    message(FATAL_ERROR "installed distribution is missing ${required_file}")
  endif()
endforeach()
set(mmay_installed_source
  "${mmay_prefix}/share/doc/mmay/${MMAY_TEST_VERSION}")
set(mmay_installed_source_build "${mmay_root}/installed-source-build")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${mmay_installed_source}"
    -B "${mmay_installed_source_build}"
    -DBUILD_TESTING=OFF
  RESULT_VARIABLE source_configure_result
  OUTPUT_VARIABLE source_configure_output
  ERROR_VARIABLE source_configure_error
  ENCODING UTF-8)
if(NOT source_configure_result EQUAL 0)
  message(FATAL_ERROR
    "installed offline source kit does not configure\n${source_configure_output}\n${source_configure_error}")
endif()
foreach(forbidden IN ITEMS "bin/ma" "bin/ruda" ".ma" "ma.mod")
  if(EXISTS "${mmay_prefix}/${forbidden}")
    message(FATAL_ERROR "legacy alias was installed: ${forbidden}")
  endif()
endforeach()

file(READ
  "${mmay_prefix}/share/mmay/${MMAY_TEST_VERSION}/provenance.json"
  provenance)
string(JSON provenance_schema GET "${provenance}" schema)
string(JSON provenance_version GET "${provenance}" version)
if(NOT provenance_schema STREQUAL "marimay.provenance/1"
    OR NOT provenance_version STREQUAL MMAY_TEST_VERSION)
  message(FATAL_ERROR "installed provenance identity is invalid")
endif()
file(READ "${mmay_prefix}/share/mmay/${MMAY_TEST_VERSION}/sbom.spdx.json" sbom)
string(JSON spdx_version GET "${sbom}" spdxVersion)
if(NOT spdx_version STREQUAL "SPDX-2.3")
  message(FATAL_ERROR "installed SBOM is not SPDX 2.3")
endif()

mmay_expect_exit(0 "installed version JSON"
  "${mmay_program}" version --json)
if(NOT MMAY_LAST_OUTPUT MATCHES "\"schema\":\"marimay.version/2\""
    OR NOT MMAY_LAST_OUTPUT MATCHES "\"doctor\"")
  message(FATAL_ERROR "installed version JSON omits CP39 identity")
endif()
mmay_expect_exit(0 "installed doctor JSON"
  "${mmay_program}" doctor --json)
if(NOT MMAY_LAST_OUTPUT MATCHES "\"schema\":\"marimay.doctor/1\""
    OR NOT MMAY_LAST_OUTPUT MATCHES "\"ok\":true"
    OR NOT MMAY_LAST_OUTPUT MATCHES "\"layout\":\"installed\"")
  message(FATAL_ERROR "installed doctor JSON is not healthy")
endif()

file(RENAME "${mmay_prefix}" "${mmay_relocated}")
set(mmay_prefix "${mmay_relocated}")
set(mmay_program "${mmay_prefix}/bin/mmay${MMAY_TEST_EXECUTABLE_SUFFIX}")
mmay_expect_exit(0 "relocated doctor"
  "${mmay_program}" doctor --json)
if(NOT MMAY_LAST_OUTPUT MATCHES "\"ok\":true")
  message(FATAL_ERROR "relocated distribution is unhealthy")
endif()
mmay_expect_exit(0 "bare PATH invocation"
  "${CMAKE_COMMAND}" -E env
  --modify "PATH=path_list_prepend:${mmay_prefix}/bin"
  "mmay${MMAY_TEST_EXECUTABLE_SUFFIX}" doctor --json)
if(NOT MMAY_LAST_OUTPUT MATCHES "\"ok\":true")
  message(FATAL_ERROR "PATH-resolved installed command is unhealthy")
endif()

mmay_expect_exit(0 "project initialization"
  "${mmay_program}" init "${mmay_project}")
mmay_expect_exit(0 "initialized formatter"
  "${mmay_program}" fmt --check "${mmay_project}")
mmay_expect_exit(0 "initialized checker"
  "${mmay_program}" check "${mmay_project}")
mmay_expect_exit(0 "initialized VM run"
  "${mmay_program}" run --engine vm "${mmay_project}" -- --help)
if(NOT MMAY_LAST_OUTPUT STREQUAL "Marimay initialized project\n")
  message(FATAL_ERROR "VM init output mismatch: ${MMAY_LAST_OUTPUT}")
endif()
mmay_expect_exit(0 "initialized native run"
  "${mmay_program}" run --engine native "${mmay_project}" -- --help)
if(NOT MMAY_LAST_OUTPUT STREQUAL "Marimay initialized project\n")
  message(FATAL_ERROR "native init output mismatch: ${MMAY_LAST_OUTPUT}")
endif()
mmay_expect_exit(0 "initialized VM/native tests"
  "${mmay_program}" test --engine both "${mmay_project}")
set(mmay_artifact
  "${mmay_project}/dist/alpha-smoke${MMAY_TEST_EXECUTABLE_SUFFIX}")
file(MAKE_DIRECTORY "${mmay_project}/dist")
mmay_expect_exit(0 "initialized release build"
  "${mmay_program}" build --release --output "${mmay_artifact}"
  "${mmay_project}")
mmay_expect_exit(0 "initialized release artifact"
  "${mmay_artifact}" --help)

set(mmay_args_source "${MMAY_TEST_SOURCE_DIR}/tests/public-alpha/cp31-args")
execute_process(
  COMMAND "${mmay_program}" run --engine native "${mmay_args_source}" --
    alpha "two words" "" "Мир" --
  RESULT_VARIABLE args_result
  OUTPUT_VARIABLE args_output
  ERROR_VARIABLE args_error
  ENCODING UTF-8
)
if(NOT args_result EQUAL 0
    OR NOT args_output STREQUAL "alpha\ntwo words\n\nМир\n--\n")
  message(FATAL_ERROR
    "native run changed arguments: ${args_result}/${args_output}/${args_error}")
endif()

set(mmay_stream_source "${MMAY_TEST_SOURCE_DIR}/tests/public-alpha/cp32-stdio")
set(mmay_stream_input "${mmay_root}/native-run.input")
set(mmay_stream_output "${mmay_root}/native-run.stdout")
set(mmay_stream_error "${mmay_root}/native-run.stderr")
file(WRITE "${mmay_stream_input}" "stream line\r\n")
execute_process(
  COMMAND "${mmay_program}" run --engine native "${mmay_stream_source}"
  INPUT_FILE "${mmay_stream_input}"
  OUTPUT_FILE "${mmay_stream_output}"
  ERROR_FILE "${mmay_stream_error}"
  RESULT_VARIABLE stream_result
)
file(READ "${mmay_stream_output}" stream_output HEX)
file(READ "${mmay_stream_error}" stream_error HEX)
if(NOT stream_result EQUAL 0
    OR NOT stream_output STREQUAL
      "6f7574206c696e650a73747265616d206c696e650a"
    OR NOT stream_error STREQUAL "657272206c696e650a")
  message(FATAL_ERROR
    "native run stream mismatch: ${stream_result}/${stream_output}/${stream_error}")
endif()

set(mmay_env_source
  "${MMAY_TEST_SOURCE_DIR}/tests/public-alpha/cp35-environment")
mmay_expect_exit(0 "native run denied ambient environment"
  "${CMAKE_COMMAND}" -E env MMAY_ALPHA_TEST=ambient
  "${mmay_program}" run --engine native "${mmay_env_source}")
if(NOT MMAY_LAST_OUTPUT STREQUAL "denied")
  message(FATAL_ERROR "native run exposed an ungranted environment value")
endif()
mmay_expect_exit(0 "native run granted environment"
  "${CMAKE_COMMAND}" -E env MMAY_ALPHA_TEST=granted
  "${mmay_program}" run --engine native --allow-env MMAY_ALPHA_TEST
  "${mmay_env_source}")
if(NOT MMAY_LAST_OUTPUT STREQUAL "granted")
  message(FATAL_ERROR "native run did not preserve the environment grant")
endif()

set(mmay_files_source
  "${MMAY_TEST_SOURCE_DIR}/tests/public-alpha/cp34-files")
execute_process(
  COMMAND "${mmay_program}" run --engine native "${mmay_files_source}"
  WORKING_DIRECTORY "${MMAY_TEST_SOURCE_DIR}"
  RESULT_VARIABLE files_denied_result
  OUTPUT_VARIABLE files_denied_output
  ERROR_VARIABLE files_denied_error
  ENCODING UTF-8
)
if(NOT files_denied_result EQUAL 1 OR NOT files_denied_output STREQUAL "")
  message(FATAL_ERROR
    "native run did not deny an ungranted file: ${files_denied_result}/${files_denied_output}/${files_denied_error}")
endif()
execute_process(
  COMMAND "${mmay_program}" run --engine native
    --allow-read "${MMAY_TEST_SOURCE_DIR}" "${mmay_files_source}"
  WORKING_DIRECTORY "${MMAY_TEST_SOURCE_DIR}"
  RESULT_VARIABLE files_granted_result
  OUTPUT_VARIABLE files_granted_output
  ERROR_VARIABLE files_granted_error
  ENCODING UTF-8
)
if(NOT files_granted_result EQUAL 0
    OR NOT files_granted_output STREQUAL "cp34\n"
    OR NOT files_granted_error STREQUAL "")
  message(FATAL_ERROR
    "native run changed a file grant: ${files_granted_result}/${files_granted_output}/${files_granted_error}")
endif()

set(mmay_json_source
  "${MMAY_TEST_SOURCE_DIR}/tests/public-alpha/cp38-json")
set(mmay_json_input "${mmay_json_source}/input.json")
file(READ "${mmay_json_source}/expected.json" mmay_json_expected)
execute_process(
  COMMAND "${mmay_program}" run --engine native
    --allow-read "${mmay_json_source}" "${mmay_json_source}" --
    "${mmay_json_input}"
  WORKING_DIRECTORY "${MMAY_TEST_SOURCE_DIR}"
  RESULT_VARIABLE json_result
  OUTPUT_VARIABLE json_output
  ERROR_VARIABLE json_error
  ENCODING UTF-8
)
if(NOT json_result EQUAL 0
    OR NOT json_output STREQUAL "${mmay_json_expected}"
    OR NOT json_error STREQUAL "")
  message(FATAL_ERROR
    "native run changed JSON behavior: ${json_result}/${json_output}/${json_error}")
endif()

file(GLOB debug_archives LIST_DIRECTORIES false
  "${mmay_prefix}/lib/mmay/${MMAY_TEST_VERSION}/native/debug/*mmay_native_runtime_debug*")
list(LENGTH debug_archives debug_archive_count)
if(NOT debug_archive_count EQUAL 1)
  message(FATAL_ERROR "expected one installed debug runtime archive")
endif()
list(GET debug_archives 0 debug_archive)
set(debug_backup "${debug_archive}.cp39-backup")
file(RENAME "${debug_archive}" "${debug_backup}")
mmay_expect_exit(1 "doctor missing runtime"
  "${mmay_program}" doctor --json)
if(NOT MMAY_LAST_OUTPUT MATCHES "\"debug\":\{\"status\":\"missing\"")
  message(FATAL_ERROR "doctor did not classify a missing runtime")
endif()
mmay_expect_exit(1 "native run missing runtime"
  "${mmay_program}" run --engine native "${mmay_project}" -- --help)
string(FIND "${MMAY_LAST_ERROR}" "error[B0008]" runtime_error_index)
if(runtime_error_index EQUAL -1)
  message(FATAL_ERROR "native run did not reject the missing runtime")
endif()
file(RENAME "${debug_backup}" "${debug_archive}")
file(COPY_FILE "${debug_archive}" "${debug_backup}")
file(WRITE "${debug_archive}" "stale-runtime")
mmay_expect_exit(1 "doctor stale runtime"
  "${mmay_program}" doctor --json)
if(NOT MMAY_LAST_OUTPUT MATCHES "\"debug\":\{\"status\":\"incompatible\"")
  message(FATAL_ERROR "doctor did not classify a stale runtime")
endif()
file(REMOVE "${debug_archive}")
file(RENAME "${debug_backup}" "${debug_archive}")
set(mmay_required_header "${mmay_prefix}/include/mmay/runtime/core.h")
set(mmay_required_header_backup "${mmay_required_header}.cp39-backup")
file(RENAME "${mmay_required_header}" "${mmay_required_header_backup}")
mmay_expect_exit(1 "doctor missing installed header"
  "${mmay_program}" doctor --json)
if(NOT MMAY_LAST_OUTPUT MATCHES "\"status\":\"missing\"")
  message(FATAL_ERROR "doctor did not classify a missing installed header")
endif()
file(RENAME "${mmay_required_header_backup}" "${mmay_required_header}")

set(mmay_collision "${mmay_root}/foreign collision")
file(MAKE_DIRECTORY "${mmay_collision}/bin")
file(WRITE "${mmay_collision}/bin/mmay${MMAY_TEST_EXECUTABLE_SUFFIX}" "foreign\n")
mmay_expect_exit(1 "foreign command collision"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=install"
  "-DMMAY_DISTRIBUTION_SOURCE=${mmay_staged}"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_collision}"
  -P "${mmay_lifecycle}")
file(READ "${mmay_collision}/bin/mmay${MMAY_TEST_EXECUTABLE_SUFFIX}" foreign)
if(NOT foreign STREQUAL "foreign\n")
  message(FATAL_ERROR "collision test modified the foreign command")
endif()

set(mmay_interrupted "${mmay_root}/interrupted")
file(MAKE_DIRECTORY "${mmay_interrupted}.mmay-stage")
file(WRITE "${mmay_interrupted}.mmay-stage/owner" "foreign-stage\n")
mmay_expect_exit(1 "stale install stage"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=install"
  "-DMMAY_DISTRIBUTION_SOURCE=${mmay_staged}"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_interrupted}"
  -P "${mmay_lifecycle}")
if(EXISTS "${mmay_interrupted}")
  message(FATAL_ERROR "stale-stage failure published a partial prefix")
endif()

file(MAKE_DIRECTORY "${mmay_prefix}/user area")
file(WRITE "${mmay_prefix}/user area/preserve.txt" "preserve\n")
mmay_expect_exit(0 "checksum-validated upgrade"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=upgrade"
  "-DMMAY_DISTRIBUTION_SOURCE=${mmay_staged}"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_prefix}"
  -P "${mmay_lifecycle}")
file(READ "${mmay_prefix}/user area/preserve.txt" preserved)
if(NOT preserved STREQUAL "preserve\n")
  message(FATAL_ERROR "upgrade did not preserve unrelated prefix content")
endif()

set(mmay_owned_readme
  "${mmay_prefix}/share/doc/mmay/${MMAY_TEST_VERSION}/README.md")
file(COPY_FILE "${mmay_owned_readme}" "${mmay_owned_readme}.backup")
file(APPEND "${mmay_owned_readme}" "modified\n")
mmay_expect_exit(1 "modified owned file uninstall refusal"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=uninstall"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_prefix}"
  -P "${mmay_lifecycle}")
if(NOT EXISTS "${mmay_program}")
  message(FATAL_ERROR "refused uninstall removed an owned command")
endif()
file(REMOVE "${mmay_owned_readme}")
file(RENAME "${mmay_owned_readme}.backup" "${mmay_owned_readme}")

mmay_expect_exit(0 "checksum-driven uninstall"
  "${CMAKE_COMMAND}"
  "-DMMAY_DISTRIBUTION_ACTION=uninstall"
  "-DMMAY_DISTRIBUTION_PREFIX=${mmay_prefix}"
  -P "${mmay_lifecycle}")
if(EXISTS "${mmay_program}")
  message(FATAL_ERROR "uninstall left the tool-owned command")
endif()
file(READ "${mmay_prefix}/user area/preserve.txt" preserved_after_uninstall)
if(NOT preserved_after_uninstall STREQUAL "preserve\n")
  message(FATAL_ERROR "uninstall did not preserve unrelated prefix content")
endif()

message(STATUS "CP39 distribution lifecycle and installed acceptance passed")
