if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE
    OR NOT DEFINED MMAY_TEST_WORK)
  message(FATAL_ERROR "CP42 test-helper variables are missing")
endif()
file(MAKE_DIRECTORY "${MMAY_TEST_WORK}")

function(succeeds name)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
  if(NOT status EQUAL 0 OR NOT error STREQUAL "")
    message(FATAL_ERROR "CP42 test-helper ${name}: ${status}/${output}/${error}")
  endif()
  set("observed_${name}" "${output}" PARENT_SCOPE)
endfunction()

succeeds(format "${MMAY_TEST_TOOL}" fmt "${MMAY_TEST_SOURCE}" --check)
succeeds(check "${MMAY_TEST_TOOL}" check "${MMAY_TEST_SOURCE}" --all-targets)
succeeds(tests "${MMAY_TEST_TOOL}" test "${MMAY_TEST_SOURCE}" --engine both)
if(NOT observed_tests MATCHES "3 passed; 0 failed"
    OR observed_tests MATCHES "undiscovered")
  message(FATAL_ERROR "CP42 test-helper discovery: ${observed_tests}")
endif()
succeeds(vm "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}")
foreach(profile IN ITEMS debug release)
  set(flags)
  if(profile STREQUAL "release")
    list(APPEND flags --release)
  endif()
  set(artifact "${MMAY_TEST_WORK}/${profile}${MMAY_TEST_SUFFIX}")
  succeeds(build "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}"
    ${flags} --output "${artifact}")
  succeeds(native "${artifact}")
  if(NOT observed_native STREQUAL observed_vm OR NOT observed_vm STREQUAL "")
    message(FATAL_ERROR "CP42 test-helper production entry differs")
  endif()
  file(REMOVE "${artifact}")
endforeach()

set(failure "${MMAY_TEST_WORK}/failure")
file(MAKE_DIRECTORY "${failure}")
file(COPY "${MMAY_TEST_SOURCE}/mmay.mod" "${MMAY_TEST_SOURCE}/helpers_test.my"
  DESTINATION "${failure}")
file(WRITE "${failure}/main_test.my"
  "@test fn failsHere(): void { nested(1, 2); }\n@test fn survivesHere(): void { outputHelper(); }\n")
foreach(engine IN ITEMS vm native both)
  execute_process(COMMAND "${MMAY_TEST_TOOL}" test "${failure}" --engine "${engine}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
  if(NOT status EQUAL 1 OR NOT error STREQUAL ""
      OR NOT output MATCHES "panic[[]G4004[]]: expect.equal failed: values differ at root"
      OR NOT output MATCHES "survivesHere [.][.][.] ok"
      OR NOT output MATCHES "1 passed; 1 failed"
      OR output MATCHES "undiscovered")
    message(FATAL_ERROR "CP42 test-helper failure ${engine}: ${status}/${output}/${error}")
  endif()
  if(engine STREQUAL "vm")
    set(expected "${output}")
  elseif(NOT output STREQUAL expected)
    message(FATAL_ERROR "CP42 test-helper ${engine} differs from VM: ${output}")
  endif()
endforeach()

# Eligibility belongs to the definition, even when a test calls the function.
set(production "${MMAY_TEST_WORK}/production")
file(MAKE_DIRECTORY "${production}")
file(COPY "${MMAY_TEST_SOURCE}/mmay.mod" DESTINATION "${production}")
file(WRITE "${production}/main.my"
  "fn helper(): void { expect.true(true); }\nfn main(): void {}\n")
file(WRITE "${production}/main_test.my" "@test fn usesHelper(): void { helper(); }\n")
set(rejected "${MMAY_TEST_WORK}/rejected${MMAY_TEST_SUFFIX}")
file(REMOVE "${rejected}")
foreach(command IN ITEMS check run build test)
  set(flags)
  if(command STREQUAL "test")
    set(flags --engine both)
  elseif(command STREQUAL "build")
    set(flags --output "${rejected}")
  endif()
  execute_process(COMMAND "${MMAY_TEST_TOOL}" "${command}" "${production}" ${flags}
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
  if(NOT status EQUAL 1 OR NOT output STREQUAL ""
      OR NOT error MATCHES "error[[]R0007[]]"
      OR NOT error MATCHES "main.my:1:21" OR EXISTS "${rejected}")
    message(FATAL_ERROR "CP42 production helper ${command}: ${status}/${output}/${error}")
  endif()
  if(command STREQUAL "check")
    set(expected "${error}")
  elseif(NOT error STREQUAL expected)
    message(FATAL_ERROR "CP42 production ${command} differs from check: ${error}")
  endif()
endforeach()
