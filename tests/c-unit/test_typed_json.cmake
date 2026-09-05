if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE
    OR NOT DEFINED MMAY_TEST_ARTIFACT)
  message(FATAL_ERROR "typed JSON test variables are missing")
endif()

function(typed_json_success name)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT status EQUAL 0 OR NOT error STREQUAL "")
    message(FATAL_ERROR "typed JSON ${name}: ${status}; ${output}; ${error}")
  endif()
  set("typed_json_${name}" "${output}" PARENT_SCOPE)
endfunction()

typed_json_success(format "${MMAY_TEST_TOOL}" fmt "${MMAY_TEST_SOURCE}" --check)
typed_json_success(check "${MMAY_TEST_TOOL}" check "${MMAY_TEST_SOURCE}")
typed_json_success(tests "${MMAY_TEST_TOOL}" test "${MMAY_TEST_SOURCE}" --engine both)
if(NOT typed_json_tests MATCHES "4 passed; 0 failed")
  message(FATAL_ERROR "typed JSON tests were not executed: ${typed_json_tests}")
endif()
typed_json_success(vm "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}")
set(expected "{\"version\":1,\"nextId\":2,\"notes\":[{\"id\":1,\"text\":\"Привет 🌿\",\"tags\":[\"alpha\"]}]}")
if(NOT typed_json_vm STREQUAL expected)
  message(FATAL_ERROR "typed JSON VM output: ${typed_json_vm}")
endif()
foreach(profile IN ITEMS debug release)
  set(flags)
  if(profile STREQUAL "release")
    list(APPEND flags --release)
  endif()
  set(artifact "${MMAY_TEST_ARTIFACT}-${profile}")
  typed_json_success(build "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}"
    ${flags} --output "${artifact}")
  typed_json_success(native "${artifact}")
  if(NOT typed_json_native STREQUAL expected)
    message(FATAL_ERROR "typed JSON ${profile} native output: ${typed_json_native}")
  endif()
  foreach(input IN ITEMS "{}"
      "{\"version\":1,\"nextId\":1,\"notes\":null}"
      "{\"version\":1,\"nextId\":1,\"notes\":[],\"extra\":0}"
      "{\"version\":1,\"version\":1,\"nextId\":1,\"notes\":[]}")
    execute_process(COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" -- "${input}"
      RESULT_VARIABLE vm_status OUTPUT_VARIABLE vm_output ERROR_VARIABLE vm_error)
    execute_process(COMMAND "${artifact}" "${input}"
      RESULT_VARIABLE native_status OUTPUT_VARIABLE native_output ERROR_VARIABLE native_error)
    if(NOT vm_status EQUAL 1 OR NOT native_status EQUAL 1
        OR NOT vm_output STREQUAL "" OR NOT native_output STREQUAL ""
        OR vm_error STREQUAL "" OR NOT vm_error STREQUAL native_error)
      message(FATAL_ERROR "typed JSON rejection differs: ${vm_status}/${vm_output}/${vm_error}; ${native_status}/${native_output}/${native_error}")
    endif()
  endforeach()
endforeach()
