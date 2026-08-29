if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_ENV_SOURCE
    OR NOT DEFINED MMAY_TEST_PROCESS_SOURCE
    OR NOT DEFINED MMAY_TEST_ENV_ARTIFACT
    OR NOT DEFINED MMAY_TEST_PROCESS_ARTIFACT)
  message(FATAL_ERROR "CP35 process test variables are missing")
endif()

set(cp35_directory "${CMAKE_CURRENT_BINARY_DIR}/cp35 process ü")
set(cp35_working "${cp35_directory}/working directory ü")
set(cp35_relocated "${cp35_directory}/relocated tools ü")
get_filename_component(cp35_process_artifact_name
  "${MMAY_TEST_PROCESS_ARTIFACT}" NAME
)
set(cp35_process_relocated
  "${cp35_relocated}/${cp35_process_artifact_name}"
)
set(cp35_emit_first "${cp35_directory}/first.c")
set(cp35_emit_second "${cp35_directory}/second.c")
set(cp35_emit_other "${cp35_directory}/other.c")
file(REMOVE_RECURSE "${cp35_directory}")
file(REMOVE "${MMAY_TEST_ENV_ARTIFACT}" "${MMAY_TEST_PROCESS_ARTIFACT}")
file(MAKE_DIRECTORY "${cp35_working}" "${cp35_relocated}")

set(cp35_build_secret "CP35_BUILD_SECRET_DO_NOT_EMBED")
set(cp35_runtime_value "runtime value ü with spaces")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "MMAY_ALPHA_TEST=${cp35_build_secret}"
    "${MMAY_TEST_TOOL}" build "${MMAY_TEST_ENV_SOURCE}"
    --allow-env MMAY_ALPHA_TEST --output "${MMAY_TEST_ENV_ARTIFACT}"
  RESULT_VARIABLE cp35_build_status
  OUTPUT_VARIABLE cp35_build_output
  ERROR_VARIABLE cp35_build_error
)
if(NOT cp35_build_status EQUAL 0)
  message(FATAL_ERROR
    "CP35 native environment build failed (${cp35_build_status}):\n${cp35_build_output}${cp35_build_error}"
  )
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "MMAY_ALPHA_TEST=${cp35_runtime_value}"
    "${MMAY_TEST_TOOL}" run "${MMAY_TEST_ENV_SOURCE}"
    --allow-env MMAY_ALPHA_TEST
  WORKING_DIRECTORY "${cp35_working}"
  RESULT_VARIABLE cp35_vm_status
  OUTPUT_VARIABLE cp35_vm_output
  ERROR_VARIABLE cp35_vm_error
)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "MMAY_ALPHA_TEST=${cp35_runtime_value}"
    "${MMAY_TEST_ENV_ARTIFACT}"
  WORKING_DIRECTORY "${cp35_working}"
  RESULT_VARIABLE cp35_native_status
  OUTPUT_VARIABLE cp35_native_output
  ERROR_VARIABLE cp35_native_error
)
if(NOT cp35_vm_status EQUAL 0 OR NOT cp35_native_status EQUAL 0
    OR NOT cp35_vm_output STREQUAL cp35_runtime_value
    OR NOT cp35_native_output STREQUAL cp35_runtime_value
    OR NOT cp35_vm_error STREQUAL "" OR NOT cp35_native_error STREQUAL "")
  message(FATAL_ERROR
    "CP35 environment VM/native divergence: vm=${cp35_vm_status}/${cp35_vm_output}/${cp35_vm_error}; native=${cp35_native_status}/${cp35_native_output}/${cp35_native_error}"
  )
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env --unset=MMAY_ALPHA_TEST
    "${MMAY_TEST_TOOL}" run "${MMAY_TEST_ENV_SOURCE}"
    --allow-env MMAY_ALPHA_TEST
  RESULT_VARIABLE cp35_missing_status
  OUTPUT_VARIABLE cp35_missing_output
  ERROR_VARIABLE cp35_missing_error
)
if(NOT cp35_missing_status EQUAL 0 OR NOT cp35_missing_output STREQUAL "missing"
    OR NOT cp35_missing_error STREQUAL "")
  message(FATAL_ERROR
    "CP35 missing environment mismatch: ${cp35_missing_status}/${cp35_missing_output}/${cp35_missing_error}"
  )
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env "MMAY_ALPHA_TEST="
    "${MMAY_TEST_TOOL}" run "${MMAY_TEST_ENV_SOURCE}"
    --allow-env MMAY_ALPHA_TEST
  RESULT_VARIABLE cp35_empty_status
  OUTPUT_VARIABLE cp35_empty_output
  ERROR_VARIABLE cp35_empty_error
)
if(NOT cp35_empty_status EQUAL 0 OR NOT cp35_empty_output STREQUAL ""
    OR NOT cp35_empty_error STREQUAL "")
  message(FATAL_ERROR
    "CP35 empty environment mismatch: ${cp35_empty_status}/${cp35_empty_output}/${cp35_empty_error}"
  )
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "MMAY_ALPHA_TEST=${cp35_build_secret}"
    "${MMAY_TEST_TOOL}" run "${MMAY_TEST_ENV_SOURCE}"
  RESULT_VARIABLE cp35_denied_status
  OUTPUT_VARIABLE cp35_denied_output
  ERROR_VARIABLE cp35_denied_error
)
if(NOT cp35_denied_status EQUAL 0 OR NOT cp35_denied_output STREQUAL "denied"
    OR NOT cp35_denied_error STREQUAL ""
    OR cp35_denied_output MATCHES "${cp35_build_secret}")
  message(FATAL_ERROR
    "CP35 denied environment leaked data: ${cp35_denied_status}/${cp35_denied_output}/${cp35_denied_error}"
  )
endif()

foreach(pair IN ITEMS first second)
  if(pair STREQUAL "first")
    set(cp35_emit_value "first host value")
  else()
    set(cp35_emit_value "second host value")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MMAY_ALPHA_TEST=${cp35_emit_value}"
      "${MMAY_TEST_TOOL}" build "${MMAY_TEST_ENV_SOURCE}"
      --allow-env MMAY_ALPHA_TEST --emit c
      --output "${cp35_directory}/${pair}.c"
    RESULT_VARIABLE cp35_emit_status
    OUTPUT_VARIABLE cp35_emit_output
    ERROR_VARIABLE cp35_emit_error
  )
  if(NOT cp35_emit_status EQUAL 0)
    message(FATAL_ERROR
      "CP35 C emission ${pair} failed: ${cp35_emit_output}${cp35_emit_error}"
    )
  endif()
endforeach()
execute_process(
  COMMAND "${MMAY_TEST_TOOL}" build "${MMAY_TEST_ENV_SOURCE}"
    --allow-env OTHER_ENV --emit c --output "${cp35_emit_other}"
  RESULT_VARIABLE cp35_other_status
  OUTPUT_VARIABLE cp35_other_output
  ERROR_VARIABLE cp35_other_error
)
if(NOT cp35_other_status EQUAL 0)
  message(FATAL_ERROR
    "CP35 alternate grant emission failed: ${cp35_other_output}${cp35_other_error}"
  )
endif()
file(SHA256 "${cp35_emit_first}" cp35_first_hash)
file(SHA256 "${cp35_emit_second}" cp35_second_hash)
file(SHA256 "${cp35_emit_other}" cp35_other_hash)
file(READ "${cp35_emit_first}" cp35_emitted_source)
if(NOT cp35_first_hash STREQUAL cp35_second_hash
    OR cp35_first_hash STREQUAL cp35_other_hash
    OR cp35_emitted_source MATCHES "first host value"
    OR cp35_emitted_source MATCHES "second host value"
    OR cp35_emitted_source MATCHES "${cp35_build_secret}")
  message(FATAL_ERROR
    "CP35 native policy/cache inputs include a host value or omit grant names"
  )
endif()

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" build "${MMAY_TEST_PROCESS_SOURCE}"
    --output "${MMAY_TEST_PROCESS_ARTIFACT}"
  RESULT_VARIABLE cp35_process_build_status
  OUTPUT_VARIABLE cp35_process_build_output
  ERROR_VARIABLE cp35_process_build_error
)
if(NOT cp35_process_build_status EQUAL 0)
  message(FATAL_ERROR
    "CP35 process-context build failed: ${cp35_process_build_output}${cp35_process_build_error}"
  )
endif()
file(RENAME "${MMAY_TEST_PROCESS_ARTIFACT}" "${cp35_process_relocated}")
execute_process(
  COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_PROCESS_SOURCE}"
  WORKING_DIRECTORY "${cp35_working}"
  RESULT_VARIABLE cp35_process_vm_status
  OUTPUT_VARIABLE cp35_process_vm_output
  ERROR_VARIABLE cp35_process_vm_error
)
execute_process(
  COMMAND "${cp35_process_relocated}"
  WORKING_DIRECTORY "${cp35_working}"
  RESULT_VARIABLE cp35_process_native_status
  OUTPUT_VARIABLE cp35_process_native_output
  ERROR_VARIABLE cp35_process_native_error
)
if(WIN32)
  if(NOT cp35_process_vm_output MATCHES "^[A-Za-z]:"
      OR NOT cp35_process_native_output MATCHES "^[A-Za-z]:")
    message(FATAL_ERROR "CP35 Windows process paths are not absolute")
  endif()
else()
  file(REAL_PATH "${cp35_working}" cp35_working_real)
  file(REAL_PATH "${MMAY_TEST_TOOL}" cp35_tool_real)
  file(REAL_PATH "${cp35_process_relocated}" cp35_relocated_real)
  set(cp35_expected_vm "${cp35_working_real}\n${cp35_tool_real}\n")
  set(cp35_expected_native
    "${cp35_working_real}\n${cp35_relocated_real}\n"
  )
  if(NOT cp35_process_vm_output STREQUAL cp35_expected_vm
      OR NOT cp35_process_native_output STREQUAL cp35_expected_native)
    message(FATAL_ERROR
      "CP35 process identities mismatch: vm=${cp35_process_vm_output}; native=${cp35_process_native_output}"
    )
  endif()
endif()
if(NOT cp35_process_vm_status EQUAL 0
    OR NOT cp35_process_native_status EQUAL 0
    OR NOT cp35_process_vm_error STREQUAL ""
    OR NOT cp35_process_native_error STREQUAL "")
  message(FATAL_ERROR
    "CP35 process context failed: vm=${cp35_process_vm_status}/${cp35_process_vm_error}; native=${cp35_process_native_status}/${cp35_process_native_error}"
  )
endif()

function(cp35_expect_cli_error name expected)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT status EQUAL 2 OR NOT output STREQUAL ""
      OR NOT error STREQUAL "${expected}")
    message(FATAL_ERROR
      "CP35 CLI ${name}: status=${status}; stdout=${output}; stderr=${error}"
    )
  endif()
endfunction()

cp35_expect_cli_error(
  missing_name
  "error[U0004]: option '--allow-env' requires a value\n"
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_ENV_SOURCE}" --allow-env
)
cp35_expect_cli_error(
  empty_name
  "error[U0005]: environment grant must be a unique ASCII name and stay within the 64-name limit\n"
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_ENV_SOURCE}" --allow-env=
)
cp35_expect_cli_error(
  invalid_name
  "error[U0005]: environment grant must be a unique ASCII name and stay within the 64-name limit\n"
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_ENV_SOURCE}" --allow-env BAD-NAME
)
cp35_expect_cli_error(
  duplicate_name
  "error[U0005]: environment grant must be a unique ASCII name and stay within the 64-name limit\n"
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_ENV_SOURCE}"
    --allow-env DUPLICATE --allow-env DUPLICATE
)
cp35_expect_cli_error(
  unsupported_command
  "error[U0002]: unknown option '--allow-env'\n"
  "${MMAY_TEST_TOOL}" check "${MMAY_TEST_ENV_SOURCE}"
    --allow-env MMAY_ALPHA_TEST
)
set(cp35_excess_names "${MMAY_TEST_TOOL}" run "${MMAY_TEST_ENV_SOURCE}")
foreach(index RANGE 0 64)
  list(APPEND cp35_excess_names --allow-env "MMAY_CP35_${index}")
endforeach()
cp35_expect_cli_error(
  excess_names
  "error[U0005]: environment grant must be a unique ASCII name and stay within the 64-name limit\n"
  ${cp35_excess_names}
)

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" help run
  RESULT_VARIABLE cp35_help_status
  OUTPUT_VARIABLE cp35_help_output
  ERROR_VARIABLE cp35_help_error
)
if(NOT cp35_help_status EQUAL 0 OR NOT cp35_help_error STREQUAL ""
    OR NOT cp35_help_output MATCHES "--allow-env name")
  message(FATAL_ERROR
    "CP35 run help mismatch: ${cp35_help_status}/${cp35_help_output}/${cp35_help_error}"
  )
endif()

file(REMOVE "${MMAY_TEST_ENV_ARTIFACT}" "${MMAY_TEST_PROCESS_ARTIFACT}")
