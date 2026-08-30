if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE
    OR NOT DEFINED MMAY_TEST_ARTIFACT)
  message(FATAL_ERROR "CP37 CLI parser test variables are missing")
endif()

file(REMOVE "${MMAY_TEST_ARTIFACT}")

function(cp37_expect_success name)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT status EQUAL 0 OR NOT error STREQUAL "")
    message(FATAL_ERROR
      "CP37 ${name} failed: status=${status}; stdout=${output}; stderr=${error}"
    )
  endif()
  set("cp37_${name}_output" "${output}" PARENT_SCOPE)
endfunction()

function(cp37_capture name)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  set("cp37_${name}_status" "${status}" PARENT_SCOPE)
  set("cp37_${name}_output" "${output}" PARENT_SCOPE)
  set("cp37_${name}_error" "${error}" PARENT_SCOPE)
endfunction()

cp37_expect_success(
  format
  "${MMAY_TEST_TOOL}" fmt "${MMAY_TEST_SOURCE}" --check
)
cp37_expect_success(
  check
  "${MMAY_TEST_TOOL}" check "${MMAY_TEST_SOURCE}"
)
cp37_expect_success(
  tests
  "${MMAY_TEST_TOOL}" test "${MMAY_TEST_SOURCE}" --engine both
)
cp37_expect_success(
  build
  "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}" --release
    --output "${MMAY_TEST_ARTIFACT}"
)

cp37_capture(
  vm_help
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" -- --help
)
cp37_capture(native_help "${MMAY_TEST_ARTIFACT}" --help)
cp37_capture(
  vm_count
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" --
    --count 3 input.txt
)
cp37_capture(native_count "${MMAY_TEST_ARTIFACT}" --count 3 input.txt)
cp37_capture(
  vm_unknown
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" -- --unknown
)
cp37_capture(native_unknown "${MMAY_TEST_ARTIFACT}" --unknown)
cp37_capture(
  vm_missing
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" -- --count
)
cp37_capture(native_missing "${MMAY_TEST_ARTIFACT}" --count)
cp37_capture(
  vm_heap_limit
  "${MMAY_TEST_TOOL}" run --max-heap-bytes 1024
    "${MMAY_TEST_SOURCE}" -- --count 3 input.txt
)

set(expected_help [=[Usage: cp37-demo [options] [<input>] [<extra>...]

    Parse a bounded deterministic command line.

Options:
  -h, --help
    Show this help text.
  -v, --verbose
    Increase diagnostic detail.
  -q, --quiet
    Suppress ordinary output.
  -c, --count <COUNT>
    Select a signed item count.
  -C, --color[=<WHEN>]
    Enable color with an optional policy.
  -t, --tag <TAG>
    Attach a repeatable tag.

Arguments:
  [<input>]
    Input path to process.
  [<extra>...]
    Additional input values.
]=])

if(NOT cp37_format_output STREQUAL ""
    OR NOT cp37_check_output STREQUAL ""
    OR NOT cp37_tests_output MATCHES "12 passed; 0 failed")
  message(FATAL_ERROR
    "CP37 command output mismatch: fmt=${cp37_format_output}; check=${cp37_check_output}; tests=${cp37_tests_output}"
  )
endif()
if(NOT cp37_vm_help_status EQUAL 0
    OR NOT cp37_native_help_status EQUAL 0
    OR NOT cp37_vm_help_error STREQUAL ""
    OR NOT cp37_native_help_error STREQUAL ""
    OR NOT cp37_vm_help_output STREQUAL expected_help
    OR NOT cp37_native_help_output STREQUAL expected_help)
  message(FATAL_ERROR
    "CP37 exact help mismatch: vm=${cp37_vm_help_status}:${cp37_vm_help_output}:${cp37_vm_help_error}; native=${cp37_native_help_status}:${cp37_native_help_output}:${cp37_native_help_error}"
  )
endif()
if(NOT cp37_vm_count_status EQUAL 0
    OR NOT cp37_native_count_status EQUAL 0
    OR NOT cp37_vm_count_output STREQUAL "count=3 input=input.txt\n"
    OR NOT cp37_native_count_output STREQUAL cp37_vm_count_output
    OR NOT cp37_vm_count_error STREQUAL ""
    OR NOT cp37_native_count_error STREQUAL "")
  message(FATAL_ERROR "CP37 count VM/native mismatch")
endif()
if(NOT cp37_vm_unknown_status EQUAL 2
    OR NOT cp37_native_unknown_status EQUAL 2
    OR NOT cp37_vm_unknown_output STREQUAL ""
    OR NOT cp37_native_unknown_output STREQUAL ""
    OR NOT cp37_vm_unknown_error STREQUAL
      "unknown option --unknown at argument 0\n"
    OR NOT cp37_native_unknown_error STREQUAL cp37_vm_unknown_error)
  message(FATAL_ERROR "CP37 unknown-option VM/native mismatch")
endif()
if(NOT cp37_vm_missing_status EQUAL 2
    OR NOT cp37_native_missing_status EQUAL 2
    OR NOT cp37_vm_missing_output STREQUAL ""
    OR NOT cp37_native_missing_output STREQUAL ""
    OR NOT cp37_vm_missing_error STREQUAL
      "option --count requires a value at argument 0\n"
    OR NOT cp37_native_missing_error STREQUAL cp37_vm_missing_error)
  message(FATAL_ERROR "CP37 missing-value VM/native mismatch")
endif()
if(NOT cp37_vm_heap_limit_status EQUAL 70
    OR NOT cp37_vm_heap_limit_output STREQUAL ""
    OR NOT cp37_vm_heap_limit_error MATCHES
      "^error\\[I0002\\]: managed heap budget exceeded: live=[0-9]+ bytes, requested=[0-9]+ bytes, limit=1024 bytes\n$")
  message(FATAL_ERROR
    "CP37 heap-limit mismatch: status=${cp37_vm_heap_limit_status}; stdout=${cp37_vm_heap_limit_output}; stderr=${cp37_vm_heap_limit_error}"
  )
endif()

file(REMOVE "${MMAY_TEST_ARTIFACT}")
