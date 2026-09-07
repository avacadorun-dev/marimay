if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE
    OR NOT DEFINED MMAY_TEST_WORK)
  message(FATAL_ERROR "CP42 mapper-context variables are missing")
endif()
file(MAKE_DIRECTORY "${MMAY_TEST_WORK}")

function(succeeds name)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
  if(NOT status EQUAL 0 OR NOT error STREQUAL "")
    message(FATAL_ERROR "CP42 mapper ${name}: ${status}/${output}/${error}")
  endif()
  set("observed_${name}" "${output}" PARENT_SCOPE)
endfunction()

succeeds(format "${MMAY_TEST_TOOL}" fmt "${MMAY_TEST_SOURCE}" --check)
succeeds(check "${MMAY_TEST_TOOL}" check "${MMAY_TEST_SOURCE}" --all-targets)
succeeds(tests "${MMAY_TEST_TOOL}" test "${MMAY_TEST_SOURCE}" --engine both)
if(NOT observed_tests MATCHES "5 passed; 0 failed")
  message(FATAL_ERROR "CP42 mapper tests not executed: ${observed_tests}")
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
    message(FATAL_ERROR "CP42 mapper native ${profile} differs")
  endif()
  file(REMOVE "${artifact}")
endforeach()

function(rejects name source)
  set(root "${MMAY_TEST_WORK}/${name}")
  file(MAKE_DIRECTORY "${root}")
  file(COPY "${MMAY_TEST_SOURCE}/mmay.mod" "${MMAY_TEST_SOURCE}/mappers"
    DESTINATION "${root}")
  file(WRITE "${root}/main.my" "${source}\nfn main(): void {}\n")
  set(artifact "${root}/rejected${MMAY_TEST_SUFFIX}")
  file(REMOVE "${artifact}")
  foreach(command IN ITEMS check run build)
    set(flags)
    if(command STREQUAL "build")
      set(flags --output "${artifact}")
    endif()
    execute_process(COMMAND "${MMAY_TEST_TOOL}" "${command}" "${root}" ${flags}
      RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
    if(NOT status EQUAL 1 OR NOT output STREQUAL ""
        OR NOT error MATCHES "error[[]T0002[]]" OR EXISTS "${artifact}")
      message(FATAL_ERROR "CP42 mapper ${name}/${command}: ${status}/${output}/${error}")
    endif()
    if(command STREQUAL "check")
      set(expected "${error}")
    elseif(NOT error STREQUAL expected)
      message(FATAL_ERROR "CP42 mapper ${name}/${command} differs from check: ${error}")
    endif()
  endforeach()
endfunction()

rejects(shadowed-namespace
  "import * as m from \"tests.public-alpha.cp42-map-error-context/mappers\"; fn bad(value: Result<int,m.Input>): Result<int,string> { const m = 0; return value.mapError(m.message); }")
rejects(shadowed-name
  "fn describe(value: int): string { return \"mapped\"; } fn bad(value: Result<int,int>): Result<int,string> { const describe = 0; return value.mapError(describe); }")
rejects(lookalike-map
  "enum Pretend<T,E> { Good(T), Bad(E), } fn describe(value: int): string { return \"mapped\"; } fn bad(value: Pretend<int,int>): Pretend<int,string> { return value.mapError(describe); }")
rejects(lookalike-try
  "enum Pretend<T,E> { Good(T), Bad(E), } fn bad(value: Pretend<int,string>): Pretend<int,string> { const count = value?; return value; }")
rejects(inner-context
  "fn bad(value: Result<int,int>): Result<int,string> { const count = value.mapError((error) => \"mapped\").mapError((error) => error)?; return Ok(count); }")
rejects(imported-result-context
  "import * as m from \"tests.public-alpha.cp42-map-error-context/mappers\"; fn bad(value: Result<int,m.Input>): m.Pretend<int,string> { const count = value.mapError((error): string => m.message(error))?; return m.Pretend.Good(count); }")
