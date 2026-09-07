if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE
    OR NOT DEFINED MMAY_TEST_WORK)
  message(FATAL_ERROR "CP42 void-match test variables are missing")
endif()
file(MAKE_DIRECTORY "${MMAY_TEST_WORK}")

function(succeeds name)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
  if(NOT status EQUAL 0 OR NOT error STREQUAL "")
    message(FATAL_ERROR "CP42 ${name}: ${status}/${output}/${error}")
  endif()
  set("observed_${name}" "${output}" PARENT_SCOPE)
endfunction()

succeeds(format "${MMAY_TEST_TOOL}" fmt "${MMAY_TEST_SOURCE}" --check)
succeeds(check "${MMAY_TEST_TOOL}" check "${MMAY_TEST_SOURCE}")
# Cleanup allocates more than this budget, so reference tails must remain
# rooted across a collection. The budget is identical in both engines.
set(budget --max-heap-bytes 65536)
succeeds(vm "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}" ${budget})
succeeds(tests "${MMAY_TEST_TOOL}" test "${MMAY_TEST_SOURCE}"
  --engine both ${budget})
if(NOT observed_tests MATCHES "5 passed; 0 failed")
  message(FATAL_ERROR "CP42 tests not executed: ${observed_tests}")
endif()
if(NOT observed_format STREQUAL "" OR NOT observed_check STREQUAL ""
    OR NOT observed_vm STREQUAL "")
  message(FATAL_ERROR "CP42 unexpected output: ${observed_format}/${observed_check}/${observed_vm}")
endif()
foreach(profile IN ITEMS debug release)
  set(flags)
  if(profile STREQUAL "release")
    list(APPEND flags --release)
  endif()
  set(artifact "${MMAY_TEST_WORK}/${profile}")
  succeeds(build "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}"
    ${flags} ${budget} --output "${artifact}")
  succeeds(native "${artifact}")
  if(NOT observed_native STREQUAL "")
    message(FATAL_ERROR "CP42 ${profile} unexpected output: ${observed_native}")
  endif()
  file(REMOVE "${artifact}")
endforeach()

function(rejects name code source)
  set(root "${MMAY_TEST_WORK}/${name}")
  file(MAKE_DIRECTORY "${root}")
  file(WRITE "${root}/mmay.mod" "module \"tests.cp42.${name}\"\nlanguage \"0.1\"\n")
  file(WRITE "${root}/main.my" "${source}\n")
  set(artifact "${root}/rejected")
  file(REMOVE "${artifact}")
  foreach(command IN ITEMS check run build)
    set(flags)
    if(command STREQUAL "build")
      set(flags --output "${artifact}")
    endif()
    execute_process(COMMAND "${MMAY_TEST_TOOL}" "${command}" "${root}" ${flags}
      RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
    if(NOT status EQUAL 1 OR NOT output STREQUAL ""
        OR NOT error MATCHES "error[[]${code}[]]" OR EXISTS "${artifact}")
      message(FATAL_ERROR "CP42 ${name}/${command}: ${status}/${output}/${error}")
    endif()
    if(command STREQUAL "check")
      set(expected "${error}")
    elseif(NOT error STREQUAL expected)
      message(FATAL_ERROR "CP42 ${name}/${command} differs from check: ${error}")
    endif()
  endforeach()
endfunction()

rejects(named-void T0002
  "fn main(): void { const value = match true { true => {}, false => {}, }; }")
rejects(named-call T0002
  "fn action(): void {} fn main(): void { const value = action(); }")
rejects(missing-value T0016
  "fn main(): int { return match true { true => {}, false => 1, }; }")
rejects(mixed-value T0002
  "fn main(): void { match true { true => {}, false => 1, }; }")
rejects(ignored-result T0019
  "fn result(): Result<int, string> { return Ok(1); } fn main(): void { match true { true => { result(); }, false => {}, }; }")
