if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE
    OR NOT DEFINED MMAY_TEST_ARTIFACT)
  message(FATAL_ERROR "CP36 Map/Set test variables are missing")
endif()

file(REMOVE "${MMAY_TEST_ARTIFACT}")

function(cp36_expect_success name)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT status EQUAL 0 OR NOT error STREQUAL "")
    message(FATAL_ERROR
      "CP36 ${name} failed: status=${status}; stdout=${output}; stderr=${error}"
    )
  endif()
  set("cp36_${name}_output" "${output}" PARENT_SCOPE)
endfunction()

cp36_expect_success(
  format
  "${MMAY_TEST_TOOL}" fmt "${MMAY_TEST_SOURCE}" --check
)
cp36_expect_success(
  check
  "${MMAY_TEST_TOOL}" check "${MMAY_TEST_SOURCE}"
)
cp36_expect_success(
  vm
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}"
)
cp36_expect_success(
  engines
  "${MMAY_TEST_TOOL}" test "${MMAY_TEST_SOURCE}" --engine both
)
cp36_expect_success(
  build
  "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}" --release
    --output "${MMAY_TEST_ARTIFACT}"
)
cp36_expect_success(native "${MMAY_TEST_ARTIFACT}")

if(NOT cp36_format_output STREQUAL ""
    OR NOT cp36_check_output STREQUAL ""
    OR NOT cp36_vm_output STREQUAL ""
    OR NOT cp36_native_output STREQUAL ""
    OR NOT cp36_engines_output MATCHES "10 passed; 0 failed")
  message(FATAL_ERROR
    "CP36 output mismatch: fmt=${cp36_format_output}; check=${cp36_check_output}; vm=${cp36_vm_output}; test=${cp36_engines_output}; native=${cp36_native_output}"
  )
endif()

file(REMOVE "${MMAY_TEST_ARTIFACT}")
