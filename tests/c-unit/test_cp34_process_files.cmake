if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE
    OR NOT DEFINED MMAY_TEST_CLEANUP_SOURCE
    OR NOT DEFINED MMAY_TEST_ARTIFACT
    OR NOT DEFINED MMAY_TEST_CLEANUP_ARTIFACT)
  message(FATAL_ERROR "CP34 process test variables are missing")
endif()

set(cp34_directory "${CMAKE_CURRENT_BINARY_DIR}/cp34-process-files")
set(cp34_root "${cp34_directory}/root")
set(cp34_outside "${cp34_directory}/outside.txt")
set(cp34_output "${cp34_root}/written ü.txt")
file(REMOVE_RECURSE "${cp34_directory}")
file(REMOVE "${MMAY_TEST_ARTIFACT}" "${MMAY_TEST_CLEANUP_ARTIFACT}")
file(MAKE_DIRECTORY "${cp34_root}/dir")
file(WRITE "${cp34_root}/alpha input.txt" "hello cp34\n")
string(ASCII 255 cp34_invalid_utf8)
file(WRITE "${cp34_root}/invalid.bin" "${cp34_invalid_utf8}")
file(WRITE "${cp34_outside}" "outside\n")

set(cp34_link_mode "none")
if(UNIX)
  file(CREATE_LINK
    "${cp34_root}/alpha input.txt"
    "${cp34_root}/link"
    SYMBOLIC RESULT cp34_link_status
  )
  if(NOT cp34_link_status STREQUAL "0")
    message(FATAL_ERROR "CP34 symlink creation failed: ${cp34_link_status}")
  endif()
  set(cp34_link_mode "link")
endif()

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}"
    --allow-read "${cp34_root}"
    --allow-read "${cp34_root}/dir"
    --allow-write "${cp34_root}"
    --output "${MMAY_TEST_ARTIFACT}"
  RESULT_VARIABLE cp34_build_status
  OUTPUT_VARIABLE cp34_build_output
  ERROR_VARIABLE cp34_build_error
)
if(NOT cp34_build_status EQUAL 0)
  message(FATAL_ERROR
    "CP34 native build failed (${cp34_build_status}):\n${cp34_build_output}${cp34_build_error}"
  )
endif()

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" build "${MMAY_TEST_CLEANUP_SOURCE}"
    --allow-read "${cp34_root}"
    --output "${MMAY_TEST_CLEANUP_ARTIFACT}"
  RESULT_VARIABLE cp34_cleanup_build_status
  OUTPUT_VARIABLE cp34_cleanup_build_output
  ERROR_VARIABLE cp34_cleanup_build_error
)
if(NOT cp34_cleanup_build_status EQUAL 0)
  message(FATAL_ERROR
    "CP34 cleanup native build failed (${cp34_cleanup_build_status}):\n${cp34_cleanup_build_output}${cp34_cleanup_build_error}"
  )
endif()

set(cp34_vm_output "${cp34_directory}/vm.stdout")
set(cp34_vm_error "${cp34_directory}/vm.stderr")
set(cp34_native_output "${cp34_directory}/native.stdout")
set(cp34_native_error "${cp34_directory}/native.stderr")
execute_process(
  COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}"
    --allow-read "${cp34_root}"
    --allow-write "${cp34_root}"
    -- "${cp34_root}" "${cp34_outside}" "${cp34_link_mode}"
  OUTPUT_FILE "${cp34_vm_output}"
  ERROR_FILE "${cp34_vm_error}"
  RESULT_VARIABLE cp34_vm_status
)
execute_process(
  COMMAND "${MMAY_TEST_ARTIFACT}"
    "${cp34_root}" "${cp34_outside}" "${cp34_link_mode}"
  OUTPUT_FILE "${cp34_native_output}"
  ERROR_FILE "${cp34_native_error}"
  RESULT_VARIABLE cp34_native_status
)

foreach(path IN ITEMS
    cp34_vm_output cp34_vm_error cp34_native_output cp34_native_error)
  file(READ "${${path}}" ${path}_hex HEX)
endforeach()
set(cp34_expected_stdout "637033342d696f0a")
if(NOT cp34_vm_status EQUAL 0 OR NOT cp34_native_status EQUAL 0
    OR NOT cp34_vm_output_hex STREQUAL cp34_expected_stdout
    OR NOT cp34_native_output_hex STREQUAL cp34_expected_stdout
    OR NOT cp34_vm_error_hex STREQUAL ""
    OR NOT cp34_native_error_hex STREQUAL ""
    OR NOT cp34_vm_output_hex STREQUAL cp34_native_output_hex
    OR NOT cp34_vm_error_hex STREQUAL cp34_native_error_hex)
  message(FATAL_ERROR
    "CP34 VM/native divergence: vm=${cp34_vm_status}/${cp34_vm_output_hex}/${cp34_vm_error_hex}; native=${cp34_native_status}/${cp34_native_output_hex}/${cp34_native_error_hex}"
  )
endif()

file(READ "${cp34_output}" cp34_written_hex HEX)
if(NOT cp34_written_hex STREQUAL "7772697474656e20637033340a")
  message(FATAL_ERROR "CP34 atomic output was ${cp34_written_hex}")
endif()
file(GLOB cp34_temporaries "${cp34_output}.mmay-tmp-*")
if(cp34_temporaries)
  message(FATAL_ERROR "CP34 left temporary files: ${cp34_temporaries}")
endif()

set(cp34_cleanup_vm_output "${cp34_directory}/cleanup.vm.stdout")
set(cp34_cleanup_vm_error "${cp34_directory}/cleanup.vm.stderr")
set(cp34_cleanup_native_output "${cp34_directory}/cleanup.native.stdout")
set(cp34_cleanup_native_error "${cp34_directory}/cleanup.native.stderr")
execute_process(
  COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_CLEANUP_SOURCE}"
    --allow-read "${cp34_root}" -- "${cp34_root}/alpha input.txt"
  OUTPUT_FILE "${cp34_cleanup_vm_output}"
  ERROR_FILE "${cp34_cleanup_vm_error}"
  RESULT_VARIABLE cp34_cleanup_vm_status
)
execute_process(
  COMMAND "${MMAY_TEST_CLEANUP_ARTIFACT}" "${cp34_root}/alpha input.txt"
  OUTPUT_FILE "${cp34_cleanup_native_output}"
  ERROR_FILE "${cp34_cleanup_native_error}"
  RESULT_VARIABLE cp34_cleanup_native_status
)
foreach(path IN ITEMS
    cp34_cleanup_vm_output cp34_cleanup_vm_error
    cp34_cleanup_native_output cp34_cleanup_native_error)
  file(READ "${${path}}" ${path}_hex HEX)
endforeach()
set(cp34_cleanup_expected
  "70616e69635b47343030305d3a2063703334207072696d6172790a"
)
if(NOT cp34_cleanup_vm_status EQUAL 1
    OR NOT cp34_cleanup_native_status EQUAL 1
    OR NOT cp34_cleanup_vm_output_hex STREQUAL ""
    OR NOT cp34_cleanup_native_output_hex STREQUAL ""
    OR NOT cp34_cleanup_vm_error_hex STREQUAL cp34_cleanup_expected
    OR NOT cp34_cleanup_native_error_hex STREQUAL cp34_cleanup_expected)
  message(FATAL_ERROR
    "CP34 cleanup divergence: vm=${cp34_cleanup_vm_status}/${cp34_cleanup_vm_output_hex}/${cp34_cleanup_vm_error_hex}; native=${cp34_cleanup_native_status}/${cp34_cleanup_native_output_hex}/${cp34_cleanup_native_error_hex}"
  )
endif()

function(cp34_expect_cli_error name expected)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT status EQUAL 2 OR NOT output STREQUAL ""
      OR NOT error STREQUAL "${expected}")
    message(FATAL_ERROR
      "CP34 CLI ${name}: status=${status}; stdout=${output}; stderr=${error}"
    )
  endif()
endfunction()

cp34_expect_cli_error(
  missing_root
  "error[U0004]: option '--allow-read' requires a value\n"
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" --allow-read
)
cp34_expect_cli_error(
  invalid_root
  "error[U0005]: filesystem capability root is not a valid normalized host path\n"
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" --allow-read /../../escape
)
cp34_expect_cli_error(
  empty_root
  "error[U0005]: filesystem capability root is empty, too long, or exceeds the 64-root limit\n"
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" --allow-read=
)
cp34_expect_cli_error(
  unsupported_command
  "error[U0002]: unknown option '--allow-read'\n"
  "${MMAY_TEST_TOOL}" check "${MMAY_TEST_SOURCE}" --allow-read "${cp34_root}"
)

set(cp34_excess_roots_command
  "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}"
)
foreach(index RANGE 0 64)
  list(APPEND cp34_excess_roots_command --allow-read "${cp34_root}")
endforeach()
cp34_expect_cli_error(
  excess_roots
  "error[U0005]: filesystem capability root is empty, too long, or exceeds the 64-root limit\n"
  ${cp34_excess_roots_command}
)

execute_process(
  COMMAND "${MMAY_TEST_TOOL}" help run
  RESULT_VARIABLE cp34_help_status
  OUTPUT_VARIABLE cp34_help_output
  ERROR_VARIABLE cp34_help_error
)
if(NOT cp34_help_status EQUAL 0 OR NOT cp34_help_error STREQUAL ""
    OR NOT cp34_help_output MATCHES "--allow-read path"
    OR NOT cp34_help_output MATCHES "--allow-write path")
  message(FATAL_ERROR
    "CP34 run help mismatch: ${cp34_help_status}/${cp34_help_output}/${cp34_help_error}"
  )
endif()

file(REMOVE "${MMAY_TEST_ARTIFACT}" "${MMAY_TEST_CLEANUP_ARTIFACT}")
