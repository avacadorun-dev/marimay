if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE
    OR NOT DEFINED MMAY_TEST_ARTIFACT OR NOT DEFINED MMAY_TEST_INPUT
    OR NOT DEFINED MMAY_TEST_EXPECTED)
  message(FATAL_ERROR "CP38 JSON test variables are missing")
endif()

set(cp38_corpus "${CMAKE_CURRENT_BINARY_DIR}/cp38-json-corpus")
set(cp38_limited "${MMAY_TEST_ARTIFACT}-limited")
set(cp38_denied "${MMAY_TEST_ARTIFACT}-denied")
file(REMOVE_RECURSE "${cp38_corpus}")
file(MAKE_DIRECTORY "${cp38_corpus}")
file(REMOVE "${MMAY_TEST_ARTIFACT}" "${cp38_limited}" "${cp38_denied}")

function(cp38_expect_success name)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT status EQUAL 0 OR NOT error STREQUAL "")
    message(FATAL_ERROR
      "CP38 ${name} failed: status=${status}; stdout=${output}; stderr=${error}"
    )
  endif()
  set("cp38_${name}_output" "${output}" PARENT_SCOPE)
endfunction()

cp38_expect_success(
  format
  "${MMAY_TEST_TOOL}" fmt "${MMAY_TEST_SOURCE}" --check
)
cp38_expect_success(
  check
  "${MMAY_TEST_TOOL}" check "${MMAY_TEST_SOURCE}"
)
cp38_expect_success(
  tests
  "${MMAY_TEST_TOOL}" test "${MMAY_TEST_SOURCE}" --engine both
)
cp38_expect_success(
  vm
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}"
    --allow-read "${MMAY_TEST_SOURCE}" -- "${MMAY_TEST_INPUT}"
)
cp38_expect_success(
  build
  "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}" --release
    --allow-read "${MMAY_TEST_SOURCE}" --allow-read "${cp38_corpus}"
    --output "${MMAY_TEST_ARTIFACT}"
)
cp38_expect_success(native "${MMAY_TEST_ARTIFACT}" "${MMAY_TEST_INPUT}")

file(READ "${MMAY_TEST_EXPECTED}" cp38_expected_output)
if(NOT cp38_expected_output MATCHES "\n$")
  string(APPEND cp38_expected_output "\n")
endif()
if(NOT cp38_format_output STREQUAL ""
    OR NOT cp38_check_output STREQUAL ""
    OR NOT cp38_tests_output MATCHES "8 passed; 0 failed"
    OR NOT cp38_vm_output STREQUAL cp38_expected_output
    OR NOT cp38_native_output STREQUAL cp38_expected_output)
  message(FATAL_ERROR
    "CP38 output mismatch: fmt=${cp38_format_output}; check=${cp38_check_output}; tests=${cp38_tests_output}; vm=${cp38_vm_output}; native=${cp38_native_output}"
  )
endif()

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" -- "${MMAY_TEST_INPUT}"
  RESULT_VARIABLE cp38_denied_vm_status
  OUTPUT_VARIABLE cp38_denied_vm_output
  ERROR_VARIABLE cp38_denied_vm_error
)
cp38_expect_success(
  denied_build
  "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}" --output "${cp38_denied}"
)
execute_process(
  COMMAND "${cp38_denied}" "${MMAY_TEST_INPUT}"
  RESULT_VARIABLE cp38_denied_native_status
  OUTPUT_VARIABLE cp38_denied_native_output
  ERROR_VARIABLE cp38_denied_native_error
)
if(NOT cp38_denied_vm_status EQUAL 1
    OR NOT cp38_denied_native_status EQUAL 1
    OR NOT cp38_denied_vm_output STREQUAL ""
    OR NOT cp38_denied_native_output STREQUAL ""
    OR NOT cp38_denied_vm_error STREQUAL cp38_denied_native_error
    OR NOT cp38_denied_vm_error MATCHES "^fs.read capability denied for .+input.json\n$")
  message(FATAL_ERROR
    "CP38 capability denial diverged: vm=${cp38_denied_vm_status}/${cp38_denied_vm_output}/${cp38_denied_vm_error}; native=${cp38_denied_native_status}/${cp38_denied_native_output}/${cp38_denied_native_error}"
  )
endif()

function(cp38_replay_rejection name content expected)
  set(path "${cp38_corpus}/${name}.json")
  file(WRITE "${path}" "${content}")
  execute_process(
    COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}"
      --allow-read "${cp38_corpus}" -- "${path}"
    RESULT_VARIABLE vm_status
    OUTPUT_VARIABLE vm_output
    ERROR_VARIABLE vm_error
  )
  execute_process(
    COMMAND "${MMAY_TEST_ARTIFACT}" "${path}"
    RESULT_VARIABLE native_status
    OUTPUT_VARIABLE native_output
    ERROR_VARIABLE native_error
  )
  if(NOT vm_status EQUAL 1 OR NOT native_status EQUAL 1
      OR NOT vm_output STREQUAL "" OR NOT native_output STREQUAL ""
      OR NOT vm_error STREQUAL native_error
      OR NOT vm_error MATCHES "${expected}")
    message(FATAL_ERROR
      "CP38 rejection ${name} diverged: vm=${vm_status}/${vm_output}/${vm_error}; native=${native_status}/${native_output}/${native_error}"
    )
  endif()
endfunction()

cp38_replay_rejection(
  empty "" "^expected an object before byte 0\n$"
)
cp38_replay_rejection(
  truncated [=[{"version":1]=]
  "^expected a comma or closing brace before byte [0-9]+\n$"
)
cp38_replay_rejection(
  duplicate [=[{"version":1,"version":2,"items":[]}]=]
  "^duplicate JSON field version at byte 13\n$"
)
cp38_replay_rejection(
  bad_escape
  [=[{"version":1,"items":[{"id":1,"name":"\x","tags":[],"labels":{},"status":{"kind":"active"}}]}]=]
  "^invalid JSON escape at byte [0-9]+\n$"
)
cp38_replay_rejection(
  bad_surrogate
  [=[{"version":1,"items":[{"id":1,"name":"\uD800x","tags":[],"labels":{},"status":{"kind":"active"}}]}]=]
  "^invalid JSON Unicode escape at byte [0-9]+\n$"
)
cp38_replay_rejection(
  integer_range [=[{"version":9223372036854775808,"items":[]}]=]
  "^JSON integer is outside the signed 64-bit range at byte [0-9]+\n$"
)
cp38_replay_rejection(
  trailing [=[{"version":1,"items":[]}{}]=]
  "^expected the end of input at byte [0-9]+\n$"
)

string(REPEAT "[" 129 cp38_open)
string(REPEAT "]" 129 cp38_close)
cp38_replay_rejection(
  nesting
  "{\"version\":1,\"items\":[],\"ignored\":${cp38_open}null${cp38_close}}"
  "^JSON nesting exceeds limit 128 at byte [0-9]+\n$"
)

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}"
    --max-heap-bytes 4096 --allow-read "${MMAY_TEST_SOURCE}"
    --output "${cp38_limited}"
  RESULT_VARIABLE cp38_limited_build_status
  OUTPUT_VARIABLE cp38_limited_build_output
  ERROR_VARIABLE cp38_limited_build_error
)
if(NOT cp38_limited_build_status EQUAL 0)
  message(FATAL_ERROR
    "CP38 limited native build failed: ${cp38_limited_build_output}${cp38_limited_build_error}"
  )
endif()
execute_process(
  COMMAND "${cp38_limited}" "${MMAY_TEST_INPUT}"
  RESULT_VARIABLE cp38_limited_status
  OUTPUT_VARIABLE cp38_limited_output
  ERROR_VARIABLE cp38_limited_error
)
if(NOT cp38_limited_status EQUAL 70
    OR NOT cp38_limited_error MATCHES "^error\\[I0002\\]: managed heap budget exceeded:")
  message(FATAL_ERROR
    "CP38 limited native result mismatch: ${cp38_limited_status}/${cp38_limited_output}/${cp38_limited_error}"
  )
endif()

file(REMOVE "${MMAY_TEST_ARTIFACT}" "${cp38_limited}" "${cp38_denied}")
file(REMOVE_RECURSE "${cp38_corpus}")
