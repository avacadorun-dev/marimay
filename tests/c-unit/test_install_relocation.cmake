set(original "${RUDA_TEST_BINARY_DIR}/cp13-install-original")
set(relocated "${RUDA_TEST_BINARY_DIR}/cp13 install relocated")
set(project "${relocated}/project with spaces")
set(tool "${relocated}/bin/ruda${RUDA_TEST_EXECUTABLE_SUFFIX}")
set(debug_output "${relocated}/cp13 debug artifact${RUDA_TEST_EXECUTABLE_SUFFIX}")
set(release_output "${relocated}/cp13 release artifact${RUDA_TEST_EXECUTABLE_SUFFIX}")
set(failed_output "${relocated}/cp13 failed artifact${RUDA_TEST_EXECUTABLE_SUFFIX}")
set(emitted_c "${relocated}/cp13 emitted.c")
set(path_output "${relocated}/cp13 PATH artifact${RUDA_TEST_EXECUTABLE_SUFFIX}")
set(release_manifest
  "${relocated}/${RUDA_TEST_LIBDIR}/ruda/${RUDA_TEST_RUNTIME_VERSION}/native/release/ruda-native-runtime.manifest"
)
set(release_archive
  "${relocated}/${RUDA_TEST_LIBDIR}/ruda/${RUDA_TEST_RUNTIME_VERSION}/native/release/${RUDA_TEST_RELEASE_ARCHIVE}"
)

file(REMOVE_RECURSE "${original}" "${relocated}")
if(RUDA_TEST_CONFIG STREQUAL "")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${RUDA_TEST_BINARY_DIR}"
      --prefix "${original}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
  )
else()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${RUDA_TEST_BINARY_DIR}"
      --config "${RUDA_TEST_CONFIG}" --prefix "${original}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
  )
endif()
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR
    "cp13 install failed (${install_result})\n${install_output}${install_error}"
  )
endif()
file(RENAME "${original}" "${relocated}")
file(MAKE_DIRECTORY "${project}")
file(COPY_FILE "${RUDA_TEST_SOURCE}" "${project}/acceptance.ruda")

function(ruda_cp13_build profile output)
  set(arguments
    "${tool}" build "${project}/acceptance.ruda"
    --output "${output}"
  )
  if(profile STREQUAL "release")
    list(APPEND arguments --release)
  endif()
  execute_process(
    COMMAND ${arguments}
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
  )
  if(NOT build_result EQUAL 0
      OR NOT build_output STREQUAL "${output}\n"
      OR NOT build_error STREQUAL "")
    message(FATAL_ERROR
      "cp13 installed ${profile} build failed (${build_result})\nstdout=${build_output}\nstderr=${build_error}"
    )
  endif()
  execute_process(
    COMMAND "${output}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
  )
  if(NOT run_result EQUAL 0
      OR NOT run_output STREQUAL "cp9 acceptance ok\n"
      OR NOT run_error STREQUAL "")
    message(FATAL_ERROR
      "cp13 installed ${profile} artifact failed (${run_result})\nstdout=${run_output}\nstderr=${run_error}"
    )
  endif()
endfunction()

ruda_cp13_build(debug "${debug_output}")
ruda_cp13_build(release "${release_output}")

if(WIN32)
  set(path_separator ";")
else()
  set(path_separator ":")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "PATH=${relocated}/bin${path_separator}$ENV{PATH}"
    "ruda${RUDA_TEST_EXECUTABLE_SUFFIX}" build
    "${project}/acceptance.ruda" --release --output "${path_output}"
  RESULT_VARIABLE path_result
  OUTPUT_VARIABLE path_stdout
  ERROR_VARIABLE path_stderr
)
if(NOT path_result EQUAL 0
    OR NOT path_stdout STREQUAL "${path_output}\n"
    OR NOT path_stderr STREQUAL ""
    OR NOT EXISTS "${path_output}")
  message(FATAL_ERROR
    "cp13 PATH-invoked relocation failed (${path_result})\nstdout=${path_stdout}\nstderr=${path_stderr}"
  )
endif()

execute_process(
  COMMAND "${tool}" build "${project}/acceptance.ruda" --release
    --emit c --output "${emitted_c}"
  RESULT_VARIABLE emit_result
  OUTPUT_VARIABLE emit_output
  ERROR_VARIABLE emit_error
)
if(NOT emit_result EQUAL 0
    OR NOT emit_output STREQUAL "${emitted_c}\n"
    OR NOT emit_error STREQUAL ""
    OR NOT EXISTS "${emitted_c}")
  message(FATAL_ERROR
    "cp13 installed emit-c failed (${emit_result})\nstdout=${emit_output}\nstderr=${emit_error}"
  )
endif()

file(READ "${release_manifest}" valid_manifest)
string(REPLACE "profile=release" "profile=debug" invalid_manifest "${valid_manifest}")
file(WRITE "${release_manifest}" "${invalid_manifest}")
execute_process(
  COMMAND "${tool}" build "${project}/acceptance.ruda" --release
    --output "${failed_output}"
  RESULT_VARIABLE failed_result
  OUTPUT_VARIABLE failed_stdout
  ERROR_VARIABLE failed_stderr
)
file(WRITE "${release_manifest}" "${valid_manifest}")
if(NOT failed_result EQUAL 1
    OR NOT failed_stdout STREQUAL ""
    OR NOT failed_stderr MATCHES "error\\[B0008\\]"
    OR failed_stderr MATCHES "(^|\\n)(ld:|link:|collect2:)"
    OR EXISTS "${failed_output}")
  message(FATAL_ERROR
    "cp13 incompatible-runtime diagnostic failed (${failed_result})\nstdout=${failed_stdout}\nstderr=${failed_stderr}"
  )
endif()

file(RENAME "${release_archive}" "${release_archive}.missing")
execute_process(
  COMMAND "${tool}" build "${project}/acceptance.ruda" --release
    --output "${failed_output}"
  RESULT_VARIABLE missing_result
  OUTPUT_VARIABLE missing_stdout
  ERROR_VARIABLE missing_stderr
)
file(RENAME "${release_archive}.missing" "${release_archive}")
if(NOT missing_result EQUAL 1
    OR NOT missing_stdout STREQUAL ""
    OR NOT missing_stderr MATCHES "error\\[B0008\\]"
    OR missing_stderr MATCHES "(^|\\n)(ld:|link:|collect2:)"
    OR EXISTS "${failed_output}")
  message(FATAL_ERROR
    "cp13 missing-runtime diagnostic failed (${missing_result})\nstdout=${missing_stdout}\nstderr=${missing_stderr}"
  )
endif()
