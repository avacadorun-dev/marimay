if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_BINARY_SOURCE
    OR NOT DEFINED MMAY_TEST_ITERATOR_SOURCE
    OR NOT DEFINED MMAY_TEST_BINARY_ARTIFACT
    OR NOT DEFINED MMAY_TEST_ITERATOR_ARTIFACT)
  message(FATAL_ERROR "CP33 process test variables are missing")
endif()

set(cp33_directory "${CMAKE_CURRENT_BINARY_DIR}/cp33-process-text-bytes")
file(MAKE_DIRECTORY "${cp33_directory}")
file(REMOVE "${MMAY_TEST_BINARY_ARTIFACT}" "${MMAY_TEST_ITERATOR_ARTIFACT}")

function(cp33_build source artifact)
  execute_process(
    COMMAND "${MMAY_TEST_TOOL}" build "${source}" --output "${artifact}"
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
  )
  if(NOT build_status EQUAL 0)
    message(FATAL_ERROR
      "CP33 native build failed (${build_status}):\n${build_output}${build_error}"
    )
  endif()
endfunction()

cp33_build("${MMAY_TEST_BINARY_SOURCE}" "${MMAY_TEST_BINARY_ARTIFACT}")
cp33_build("${MMAY_TEST_ITERATOR_SOURCE}" "${MMAY_TEST_ITERATOR_ARTIFACT}")

function(cp33_compare_binary name input expected_status expected_stdout expected_stderr)
  set(input_path "${cp33_directory}/${name}.input")
  set(vm_output "${cp33_directory}/${name}.vm.stdout")
  set(vm_error "${cp33_directory}/${name}.vm.stderr")
  set(native_output "${cp33_directory}/${name}.native.stdout")
  set(native_error "${cp33_directory}/${name}.native.stderr")
  file(WRITE "${input_path}" "${input}")
  execute_process(
    COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_BINARY_SOURCE}"
    INPUT_FILE "${input_path}"
    OUTPUT_FILE "${vm_output}"
    ERROR_FILE "${vm_error}"
    RESULT_VARIABLE vm_status
  )
  execute_process(
    COMMAND "${MMAY_TEST_BINARY_ARTIFACT}"
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
      OR NOT vm_error_hex STREQUAL native_error_hex
      OR NOT vm_output_hex STREQUAL expected_stdout
      OR NOT vm_error_hex STREQUAL expected_stderr)
    message(FATAL_ERROR
      "CP33 ${name} divergence: vm=${vm_status}/${vm_output_hex}/${vm_error_hex}; native=${native_status}/${native_output_hex}/${native_error_hex}"
    )
  endif()
endfunction()

cp33_compare_binary(partial "abc" 0 "616263" "")
cp33_compare_binary(exact "12345678" 0 "3132333435363738" "")
cp33_compare_binary(
  over_limit "123456789" 1 ""
  "6572726f725b47343030335d3a207374616e6461726420696e707574206578636565647320382d62797465206c696d69740a"
)
string(ASCII 65 255 66 cp33_binary)
cp33_compare_binary(binary "${cp33_binary}" 0 "41ff42" "")

set(iterator_vm_output "${cp33_directory}/iterator.vm.stdout")
set(iterator_vm_error "${cp33_directory}/iterator.vm.stderr")
set(iterator_native_output "${cp33_directory}/iterator.native.stdout")
set(iterator_native_error "${cp33_directory}/iterator.native.stderr")
execute_process(
  COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_ITERATOR_SOURCE}"
  OUTPUT_FILE "${iterator_vm_output}"
  ERROR_FILE "${iterator_vm_error}"
  RESULT_VARIABLE iterator_vm_status
)
execute_process(
  COMMAND "${MMAY_TEST_ITERATOR_ARTIFACT}"
  OUTPUT_FILE "${iterator_native_output}"
  ERROR_FILE "${iterator_native_error}"
  RESULT_VARIABLE iterator_native_status
)
foreach(path IN ITEMS
    iterator_vm_output iterator_vm_error
    iterator_native_output iterator_native_error)
  file(READ "${${path}}" ${path}_hex HEX)
endforeach()
set(iterator_expected
  "70616e69635b47343030355d3a206172726179206974657261746f7220696e76616c696461746564206279206d75746174696f6e0a"
)
if(NOT iterator_vm_status EQUAL 1 OR NOT iterator_native_status EQUAL 1
    OR NOT iterator_vm_output_hex STREQUAL ""
    OR NOT iterator_native_output_hex STREQUAL ""
    OR NOT iterator_vm_error_hex STREQUAL iterator_expected
    OR NOT iterator_native_error_hex STREQUAL iterator_expected)
  message(FATAL_ERROR
    "CP33 iterator divergence: vm=${iterator_vm_status}/${iterator_vm_output_hex}/${iterator_vm_error_hex}; native=${iterator_native_status}/${iterator_native_output_hex}/${iterator_native_error_hex}"
  )
endif()

file(REMOVE "${MMAY_TEST_BINARY_ARTIFACT}" "${MMAY_TEST_ITERATOR_ARTIFACT}")
