if(NOT DEFINED MMAY_TEST_TOOL OR NOT DEFINED MMAY_TEST_SOURCE)
  message(FATAL_ERROR "CLI notes test variables are missing")
endif()

# Each run owns a fresh directory; preserve failing state for investigation.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef nonce)
set(root "${CMAKE_CURRENT_BINARY_DIR}/cli-notes-${nonce}")
file(MAKE_DIRECTORY "${root}")

function(check status expected)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE actual
    OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT actual EQUAL status OR NOT output STREQUAL expected)
    message(FATAL_ERROR "notes: ${ARGN}: ${actual}/${output}/${error}")
  endif()
  if(status EQUAL 0 AND NOT error STREQUAL "")
    message(FATAL_ERROR "notes unexpected stderr: ${error}")
  endif()
endfunction()

check(0 "" "${MMAY_TEST_TOOL}" fmt "${MMAY_TEST_SOURCE}" --check)
check(0 "" "${MMAY_TEST_TOOL}" check "${MMAY_TEST_SOURCE}")
check(0 "${root}/notes\n" "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}" --release
  --allow-read "${root}" --allow-write "${root}" --output "${root}/notes")

foreach(engine vm native)
  if(engine STREQUAL "vm")
    set(command "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}"
      --allow-read "${root}" --allow-write "${root}" --)
  else()
    set(command "${root}/notes")
  endif()
  set(db "${root}/${engine}.json")
  check(0 "No notes.\n" ${command} --db "${db}" list)
  if(EXISTS "${db}")
    message(FATAL_ERROR "read-only list created a database")
  endif()
  check(0 "Added note #1.\n" ${command} --db "${db}" add "Идея 🌿" --tag work)
  check(0 "Added note #2.\n" ${command} --db "${db}" add "Buy tea")
  check(0 "#1 Идея 🌿 [work]\n#2 Buy tea\n" ${command} --db "${db}" list)
  check(0 "#1 Идея 🌿 [work]\n" ${command} --db "${db}" show 1)
  check(0 "Removed note #1.\n" ${command} --db "${db}" remove 1)
  check(0 "Added note #3.\n" ${command} --db "${db}" add "Read docs")
  file(READ "${db}" before)
  check(1 "" ${command} --db "${db}" remove 999)
  check(1 "" ${command} --db "${db}" add " ")
  file(READ "${db}" after)
  if(NOT before STREQUAL after)
    message(FATAL_ERROR "failed mutation changed the database")
  endif()
  set("${engine}_database" "${after}")
  foreach(invalid IN ITEMS
      "{\"version\":2,\"nextId\":1,\"notes\":[]}"
      "{\"version\":1,\"nextId\":0,\"notes\":[]}"
      "{\"version\":1,\"nextId\":2,\"notes\":[{\"id\":1,\"text\":\"x\",\"tags\":[]},{\"id\":1,\"text\":\"y\",\"tags\":[]}]}"
      "{\"version\":1," "{}" "[]")
    file(WRITE "${db}" "${invalid}")
    check(1 "" ${command} --db "${db}" add "Must not overwrite")
    file(READ "${db}" actual)
    if(NOT actual STREQUAL invalid)
      message(FATAL_ERROR "invalid database was overwritten")
    endif()
  endforeach()
endforeach()
if(NOT vm_database STREQUAL native_database)
  message(FATAL_ERROR "VM/native persisted bytes differ")
endif()

# Failed I/O is an expected application error, not a panic or a successful
# mutation. Use the same pathname for both engines to compare stderr exactly.
function(check_io_rejection name binary writable database)
  set(capabilities --allow-read "${root}")
  if(writable)
    list(APPEND capabilities --allow-write "${root}")
  endif()
  execute_process(COMMAND "${MMAY_TEST_TOOL}" run "${MMAY_TEST_SOURCE}"
    ${capabilities} -- --db "${database}" add "Must not be saved"
    RESULT_VARIABLE vm_status OUTPUT_VARIABLE vm_output ERROR_VARIABLE vm_error)
  execute_process(COMMAND "${binary}" --db "${database}" add "Must not be saved"
    RESULT_VARIABLE native_status OUTPUT_VARIABLE native_output ERROR_VARIABLE native_error)
  if(NOT vm_status EQUAL 1 OR NOT native_status EQUAL 1
      OR NOT vm_output STREQUAL "" OR NOT native_output STREQUAL ""
      OR vm_error STREQUAL "" OR NOT vm_error STREQUAL native_error
      OR NOT vm_error MATCHES "^marimay-notes: ")
    message(FATAL_ERROR "notes ${name}: VM=${vm_status}/${vm_output}/${vm_error}; native=${native_status}/${native_output}/${native_error}")
  endif()
endfunction()

check_io_rejection(missing_parent "${root}/notes" TRUE "${root}/absent/notes.json")
if(EXISTS "${root}/absent")
  message(FATAL_ERROR "failed write unexpectedly created the missing parent")
endif()
file(MAKE_DIRECTORY "${root}/directory.json")
check_io_rejection(directory_target "${root}/notes" TRUE "${root}/directory.json")
if(NOT IS_DIRECTORY "${root}/directory.json")
  message(FATAL_ERROR "directory target was replaced")
endif()

file(MAKE_DIRECTORY "${root}-denied")
set(denied "${root}-denied/notes.json")
file(WRITE "${denied}" "${vm_database}")
check_io_rejection(read_denied "${root}/notes" TRUE "${denied}")
file(READ "${denied}" actual)
if(NOT actual STREQUAL vm_database)
  message(FATAL_ERROR "read-denied database changed")
endif()

check(0 "${root}/notes-readonly\n" "${MMAY_TEST_TOOL}" build "${MMAY_TEST_SOURCE}" --release
  --allow-read "${root}" --output "${root}/notes-readonly")
set(readonly "${root}/readonly.json")
file(WRITE "${readonly}" "${vm_database}")
check_io_rejection(write_denied "${root}/notes-readonly" FALSE "${readonly}")
file(READ "${readonly}" actual)
if(NOT actual STREQUAL vm_database)
  message(FATAL_ERROR "write-denied database changed")
endif()
