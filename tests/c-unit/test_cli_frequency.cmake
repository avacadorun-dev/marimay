if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE)
  message(FATAL_ERROR "CLI frequency test variables are missing")
endif()
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef nonce)
set(root "${CMAKE_CURRENT_BINARY_DIR}/cli-frequency-${nonce}")
file(MAKE_DIRECTORY "${root}")
foreach(action fmt check)
  set(extra)
  if(action STREQUAL "fmt")
    set(extra --check)
  endif()
  execute_process(COMMAND "${MMAY_TEST_TOOL}" ${action} "${MMAY_TEST_SOURCE}" ${extra}
    RESULT_VARIABLE status ERROR_VARIABLE error)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "frequency ${action}: ${error}")
  endif()
endforeach()
execute_process(COMMAND "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}"
  --release --output "${root}/frequency" RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "frequency build: ${error}")
endif()
function(replay name input_path expected_status expected_output)
  execute_process(COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" -- ${ARGN}
    INPUT_FILE "${input_path}" RESULT_VARIABLE vm_status
    OUTPUT_VARIABLE vm_output ERROR_VARIABLE vm_error)
  execute_process(COMMAND "${root}/frequency" ${ARGN}
    INPUT_FILE "${input_path}" RESULT_VARIABLE native_status
    OUTPUT_VARIABLE native_output ERROR_VARIABLE native_error)
  if(NOT vm_status EQUAL expected_status OR NOT native_status EQUAL expected_status
      OR NOT vm_output STREQUAL expected_output OR NOT native_output STREQUAL expected_output
      OR NOT vm_error STREQUAL native_error)
    message(FATAL_ERROR "frequency ${name}: VM=${vm_status}/${vm_output}/${vm_error}; native=${native_status}/${native_output}/${native_error}")
  endif()
endfunction()
file(CONFIGURE OUTPUT "${root}/empty.input" CONTENT "" @ONLY NEWLINE_STYLE LF)
file(CONFIGURE OUTPUT "${root}/trim.input" CONTENT " tea \n\ttea\n\n" @ONLY NEWLINE_STYLE LF)
replay(empty "${root}/empty.input" 0 "{}\n")
replay(unicode "${CMAKE_CURRENT_LIST_DIR}/fixtures/frequency-unicode.input" 0 "{\"чай\":2,\"coffee\":1}\n")
replay(trim "${root}/trim.input" 0 "{\"tea\":2,\"\":1}\n" --trim)
replay(unknown "${root}/empty.input" 1 "" --unknown)
string(REPEAT "x" 65537 too_long)
file(CONFIGURE OUTPUT "${root}/long.input" CONTENT "${too_long}\n" @ONLY NEWLINE_STYLE LF)
replay(long_line "${root}/long.input" 1 "")
