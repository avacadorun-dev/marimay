cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE OR NOT DEFINED MMAY_TEST_WORK)
  message(FATAL_ERROR "Function-value test variables are missing")
endif()
file(REMOVE_RECURSE "${MMAY_TEST_WORK}")
file(MAKE_DIRECTORY "${MMAY_TEST_WORK}")
file(COPY "${MMAY_TEST_SOURCE}/" DESTINATION "${MMAY_TEST_WORK}/source" PATTERN ".mmay" EXCLUDE)
set(source "${MMAY_TEST_WORK}/source")
include("${CMAKE_CURRENT_LIST_DIR}/test_cp42_warning_support.cmake")
cp42_warning_baseline("${source}" 4 6)

function(callable_expect expected_status expected_output expected_error)
  cp42_warning_prefix(prefix ${ARGN})
  set(expected_error "${prefix}${expected_error}")
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 120)
  if(NOT status STREQUAL "${expected_status}" OR NOT output STREQUAL "${expected_output}"
      OR NOT error STREQUAL "${expected_error}")
    message(FATAL_ERROR "Function values: ${ARGN}: status=${status}; stdout=${output}; stderr=${error}")
  endif()
endfunction()

callable_expect(0 "" "" "${MMAY_TEST_TOOL}" fmt "${source}" --check)
callable_expect(0 "" "" "${MMAY_TEST_TOOL}" check "${source}")
execute_process(COMMAND "${MMAY_TEST_TOOL}" test "${source}" --engine both
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
if(NOT status STREQUAL "0" OR NOT error STREQUAL "${warnings_all}" OR NOT output MATCHES "15 passed; 0 failed")
  message(FATAL_ERROR "Function-value tests: status=${status}; stdout=${output}; stderr=${error}")
endif()
callable_expect(0 "" "" "${MMAY_TEST_TOOL}" run "${source}")
set(panic "panic[G4000]: callback stop\n")
callable_expect(1 "callback cleanup\n" "${panic}" "${MMAY_TEST_TOOL}" run "${source}" -- panic)
foreach(profile debug release)
  set(flags)
  if(profile STREQUAL "release")
    set(flags --release)
  endif()
  set(binary "${MMAY_TEST_WORK}/function-values-${profile}${MMAY_TEST_SUFFIX}")
  execute_process(COMMAND "${MMAY_TEST_TOOL}" build "${source}" ${flags} --output "${binary}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
  if(NOT status STREQUAL "0" OR NOT error STREQUAL "${warnings_main}")
    message(FATAL_ERROR "Function-value native build: ${status}; ${output}; ${error}")
  endif()
  callable_expect(0 "" "" "${binary}")
  callable_expect(1 "callback cleanup\n" "${panic}" "${binary}" panic)
endforeach()

# A panic in a synthetic body must retain its precise source location in both
# test observations. Missing synthetic facts must never become a silent success.
file(WRITE "${source}/failure_test.my"
  "@test\nfn closureFailure(): void {\n  const f = (): void => { expect.equal(1, 2); };\n  f();\n}\n")
foreach(engine vm native both)
  execute_process(COMMAND "${MMAY_TEST_TOOL}" test "${source}" --engine "${engine}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
  if(NOT status STREQUAL "1" OR NOT output MATCHES "15 passed; 1 failed"
      OR NOT output MATCHES "failure_test.my:3:27-3:45" OR NOT error STREQUAL "${warnings_all}")
    message(FATAL_ERROR "Function-value source location ${engine}: ${status}; ${output}; ${error}")
  endif()
endforeach()
