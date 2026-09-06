set(work "${MMAY_TEST_BINARY_DIR}/cp29-release-contract")
set(original "${work}/install-original")
set(relocated "${work}/install relocated")
set(project "${work}/cp29-quick-start")
set(artifact "${work}/cp29 quick start${MMAY_TEST_EXECUTABLE_SUFFIX}")
set(installed_tool
  "${relocated}/bin/mmay${MMAY_TEST_EXECUTABLE_SUFFIX}"
)
set(installed_metadata
  "${relocated}/${MMAY_TEST_DATADIR}/mmay/${MMAY_TEST_VERSION}/mmay-release-metadata.json"
)
set(installed_docs
  "${relocated}/${MMAY_TEST_DATADIR}/doc/mmay/${MMAY_TEST_VERSION}"
)
set(generated_metadata
  "${MMAY_TEST_BINARY_DIR}/generated/mmay-release-metadata.json"
)

file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}")
file(COPY "${MMAY_TEST_SOURCE_DIR}/tests/public-alpha/cp29-quick-start"
  DESTINATION "${work}"
)
file(READ "${generated_metadata}" expected_version_json)

set(poisoned_environment
  "HTTP_PROXY=http://127.0.0.1:1"
  "HTTPS_PROXY=http://127.0.0.1:1"
  "ALL_PROXY=http://127.0.0.1:1"
  "NO_PROXY="
)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${poisoned_environment}
    "${MMAY_TEST_TOOL}" version --json
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE version_result
  OUTPUT_VARIABLE version_output
  ERROR_VARIABLE version_error
)
if(NOT version_result EQUAL 0
    OR NOT version_output STREQUAL expected_version_json
    OR NOT version_error STREQUAL "")
  message(FATAL_ERROR
    "CP29 build-tree version mismatch (${version_result})\nstdout=${version_output}\nstderr=${version_error}"
  )
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${poisoned_environment}
    "${MMAY_TEST_TOOL}" help
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE help_result
  OUTPUT_VARIABLE help_output
  ERROR_VARIABLE help_error
)
string(FIND "${help_output}"
  "Current claim: local checker-backed slice" help_claim_index
)
string(FIND "${help_output}"
  "Staged (recognized but not executable):" help_staged_index
)
if(NOT help_result EQUAL 0
    OR NOT help_error STREQUAL ""
    OR help_claim_index LESS 0
    OR help_staged_index LESS 0)
  message(FATAL_ERROR
    "CP29 build-tree help mismatch (${help_result})\nstdout=${help_output}\nstderr=${help_error}"
  )
endif()

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" explain T0017
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE explain_result
  OUTPUT_VARIABLE explain_output
  ERROR_VARIABLE explain_error
)
string(FIND "${explain_error}" "error[U0003]" explain_code_index)
if(NOT explain_result EQUAL 2
    OR NOT explain_output STREQUAL ""
    OR explain_code_index LESS 0)
  message(FATAL_ERROR
    "CP29 staged explain mismatch (${explain_result})\nstdout=${explain_output}\nstderr=${explain_error}"
  )
endif()

foreach(arguments IN ITEMS
    "fmt;${project};--check"
    "check;${project}")
  execute_process(
    COMMAND "${MMAY_TEST_TOOL}" ${arguments}
    WORKING_DIRECTORY "${work}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
  )
  if(NOT command_result EQUAL 0
      OR NOT command_output STREQUAL ""
      OR NOT command_error STREQUAL "")
    message(FATAL_ERROR
      "CP29 quick-start command failed (${command_result}): ${arguments}\nstdout=${command_output}\nstderr=${command_error}"
    )
  endif()
endforeach()

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" run "${project}"
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE run_result
  OUTPUT_VARIABLE run_output
  ERROR_VARIABLE run_error
)
if(NOT run_result EQUAL 0
    OR NOT run_output STREQUAL "Marimay CP29 quick start\n"
    OR NOT run_error STREQUAL "")
  message(FATAL_ERROR
    "CP29 quick-start VM run failed (${run_result})\nstdout=${run_output}\nstderr=${run_error}"
  )
endif()

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" test "${project}" --engine both
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE test_result
  OUTPUT_VARIABLE test_output
  ERROR_VARIABLE test_error
)
if(NOT test_result EQUAL 0
    OR NOT test_error STREQUAL ""
    OR NOT test_output MATCHES "quickStartBehavior \.\.\. ok"
    OR NOT test_output MATCHES "quickStartOutput \.\.\. ok"
    OR NOT test_output MATCHES "2 passed; 0 failed")
  message(FATAL_ERROR
    "CP29 quick-start differential test failed (${test_result})\nstdout=${test_output}\nstderr=${test_error}"
  )
endif()

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" build "${project}" --output "${artifact}"
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error
)
if(NOT build_result EQUAL 0
    OR NOT build_output STREQUAL "${artifact}\n"
    OR NOT build_error STREQUAL ""
    OR NOT EXISTS "${artifact}")
  message(FATAL_ERROR
    "CP29 quick-start native build failed (${build_result})\nstdout=${build_output}\nstderr=${build_error}"
  )
endif()

execute_process(
  COMMAND "${artifact}"
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE artifact_result
  OUTPUT_VARIABLE artifact_output
  ERROR_VARIABLE artifact_error
)
if(NOT artifact_result EQUAL 0
    OR NOT artifact_output STREQUAL "Marimay CP29 quick start\n"
    OR NOT artifact_error STREQUAL "")
  message(FATAL_ERROR
    "CP29 quick-start artifact failed (${artifact_result})\nstdout=${artifact_output}\nstderr=${artifact_error}"
  )
endif()

if(MMAY_TEST_CONFIG STREQUAL "")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${MMAY_TEST_BINARY_DIR}"
      --prefix "${original}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
  )
else()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${MMAY_TEST_BINARY_DIR}"
      --config "${MMAY_TEST_CONFIG}" --prefix "${original}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
  )
endif()
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR
    "CP29 install failed (${install_result})\n${install_output}${install_error}"
  )
endif()
file(RENAME "${original}" "${relocated}")

foreach(required_file IN ITEMS
    "${installed_tool}"
    "${installed_metadata}"
    "${installed_docs}/README.md"
    "${installed_docs}/docs/known-limitations.md"
    "${installed_docs}/docs/releases/0.1.0-alpha.1.md"
    "${installed_docs}/docs/implementation/cli0-closure-matrix.tsv")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "CP29 installed file missing: ${required_file}")
  endif()
endforeach()
file(GLOB_RECURSE documentation_json RELATIVE "${MMAY_TEST_SOURCE_DIR}/docs"
  "${MMAY_TEST_SOURCE_DIR}/docs/*.json")
foreach(relative_path IN LISTS documentation_json)
  set(installed_json "${installed_docs}/docs/${relative_path}")
  if(NOT EXISTS "${installed_json}")
    message(FATAL_ERROR "CP29 installed evidence missing: ${relative_path}")
  endif()
  file(SHA256 "${MMAY_TEST_SOURCE_DIR}/docs/${relative_path}" source_json_hash)
  file(SHA256 "${installed_json}" installed_json_hash)
  if(NOT source_json_hash STREQUAL installed_json_hash)
    message(FATAL_ERROR "CP29 installed evidence differs: ${relative_path}")
  endif()
endforeach()
file(READ "${installed_metadata}" installed_version_json)
if(NOT installed_version_json STREQUAL expected_version_json)
  message(FATAL_ERROR "CP29 installed metadata differs from build metadata")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${poisoned_environment}
    "${installed_tool}" version --json
  WORKING_DIRECTORY "${relocated}"
  RESULT_VARIABLE installed_version_result
  OUTPUT_VARIABLE installed_version_output
  ERROR_VARIABLE installed_version_error
)
if(NOT installed_version_result EQUAL 0
    OR NOT installed_version_output STREQUAL installed_version_json
    OR NOT installed_version_error STREQUAL "")
  message(FATAL_ERROR
    "CP29 relocated version mismatch (${installed_version_result})\nstdout=${installed_version_output}\nstderr=${installed_version_error}"
  )
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${poisoned_environment}
    "${installed_tool}" help
  WORKING_DIRECTORY "${relocated}"
  RESULT_VARIABLE installed_help_result
  OUTPUT_VARIABLE installed_help_output
  ERROR_VARIABLE installed_help_error
)
if(NOT installed_help_result EQUAL 0
    OR NOT installed_help_error STREQUAL ""
    OR NOT installed_help_output MATCHES "not a public alpha")
  message(FATAL_ERROR
    "CP29 relocated help mismatch (${installed_help_result})\nstdout=${installed_help_output}\nstderr=${installed_help_error}"
  )
endif()
