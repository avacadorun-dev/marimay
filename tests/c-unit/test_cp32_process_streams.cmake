if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE
    OR NOT DEFINED MMAY_TEST_ARTIFACT)
  message(FATAL_ERROR "CP32 process test variables are missing")
endif()

set(cp32_directory "${CMAKE_CURRENT_BINARY_DIR}/cp32-process-streams")
file(MAKE_DIRECTORY "${cp32_directory}")
file(REMOVE "${MMAY_TEST_ARTIFACT}")

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}"
    --output "${MMAY_TEST_ARTIFACT}"
  RESULT_VARIABLE cp32_build_status
  OUTPUT_VARIABLE cp32_build_output
  ERROR_VARIABLE cp32_build_error
)
if(NOT cp32_build_status EQUAL 0)
  message(FATAL_ERROR
    "CP32 native build failed (${cp32_build_status}):\n${cp32_build_output}${cp32_build_error}"
  )
endif()

function(cp32_compare_case name input newline_style expected_status expected_stdout expected_stderr)
  set(input_path "${cp32_directory}/${name}.input")
  set(vm_output "${cp32_directory}/${name}.vm.stdout")
  set(vm_error "${cp32_directory}/${name}.vm.stderr")
  set(native_output "${cp32_directory}/${name}.native.stdout")
  set(native_error "${cp32_directory}/${name}.native.stderr")
  file(CONFIGURE OUTPUT "${input_path}" CONTENT "${input}" @ONLY NEWLINE_STYLE ${newline_style})
  execute_process(
    COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}"
    INPUT_FILE "${input_path}"
    OUTPUT_FILE "${vm_output}"
    ERROR_FILE "${vm_error}"
    RESULT_VARIABLE vm_status
  )
  execute_process(
    COMMAND "${MMAY_TEST_ARTIFACT}"
    INPUT_FILE "${input_path}"
    OUTPUT_FILE "${native_output}"
    ERROR_FILE "${native_error}"
    RESULT_VARIABLE native_status
  )
  foreach(path IN ITEMS vm_output vm_error native_output native_error)
    file(READ "${${path}}" ${path}_hex HEX)
  endforeach()
  if(NOT vm_status EQUAL expected_status
      OR NOT native_status EQUAL expected_status
      OR NOT vm_output_hex STREQUAL native_output_hex
      OR NOT vm_error_hex STREQUAL native_error_hex)
    message(FATAL_ERROR
      "CP32 ${name} divergence: vm=${vm_status}/${vm_output_hex}/${vm_error_hex}; native=${native_status}/${native_output_hex}/${native_error_hex}"
    )
  endif()
  if(NOT expected_stdout STREQUAL "-"
      AND NOT vm_output_hex STREQUAL expected_stdout)
    message(FATAL_ERROR "CP32 ${name} stdout was ${vm_output_hex}")
  endif()
  if(NOT expected_stderr STREQUAL "-"
      AND NOT vm_error_hex STREQUAL expected_stderr)
    message(FATAL_ERROR "CP32 ${name} stderr was ${vm_error_hex}")
  endif()
endfunction()

cp32_compare_case(
  crlf "hello\n" CRLF 0
  "6f7574206c696e650a68656c6c6f0a"
  "657272206c696e650a"
)
cp32_compare_case(
  eof "" LF 0
  "6f7574206c696e650a3c656f663e0a"
  "657272206c696e650a"
)
string(ASCII 255 cp32_invalid_utf8)
cp32_compare_case(invalid_utf8 "${cp32_invalid_utf8}\n" LF 1 "" "-")
cp32_compare_case(
  line_too_long "123456789012345678901234567890123\n" LF 1 "" "-"
)

if(UNIX)
  if(NOT DEFINED MMAY_TEST_BROKEN_PIPE_LAUNCHER)
    message(FATAL_ERROR "CP32 broken-pipe launcher is missing")
  endif()
  set(cp32_broken_input "${cp32_directory}/broken-pipe.input")
  set(cp32_broken_expected
    "657272206c696e650a6572726f725b47343030335d3a207374616e646172642073747265616d20666c757368206661696c65640a"
  )
  file(CONFIGURE OUTPUT "${cp32_broken_input}" CONTENT "pipe\n" @ONLY NEWLINE_STYLE LF)
  foreach(engine IN ITEMS vm native)
    set(cp32_broken_error "${cp32_directory}/broken-pipe.${engine}.stderr")
    if(engine STREQUAL "vm")
      set(cp32_broken_command
        "${MMAY_TEST_BROKEN_PIPE_LAUNCHER}"
        "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}"
      )
    else()
      set(cp32_broken_command
        "${MMAY_TEST_BROKEN_PIPE_LAUNCHER}"
        "${MMAY_TEST_ARTIFACT}"
      )
    endif()
    execute_process(
      COMMAND ${cp32_broken_command}
      INPUT_FILE "${cp32_broken_input}"
      ERROR_FILE "${cp32_broken_error}"
      RESULT_VARIABLE cp32_broken_child_status
    )
    file(READ "${cp32_broken_error}" cp32_broken_error_hex HEX)
    if(NOT cp32_broken_child_status EQUAL 1
        OR NOT cp32_broken_error_hex STREQUAL cp32_broken_expected)
      message(FATAL_ERROR
        "CP32 broken-pipe ${engine}: child=${cp32_broken_child_status}; stderr=${cp32_broken_error_hex}"
      )
    endif()
  endforeach()
endif()

file(REMOVE "${MMAY_TEST_ARTIFACT}")
