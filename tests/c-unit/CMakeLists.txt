add_executable(ruda_c_unit
  test_allocator.c
  test_ast.c
  test_base.c
  test_bytecode.c
  test_bytecode_vm.c
  test_checker.c
  test_checker_arrays.c
  test_cli.c
  test_compiler_service.c
  test_diagnostic.c
  test_directory.c
  test_doc_examples.c
  test_document.c
  test_file.c
  test_formatter.c
  test_hash_map.c
  test_lexer.c
  test_manifest.c
  test_managed.c
  test_mir.c
  test_mir_arrays.c
  test_mir_bytecode.c
  test_native_c_emitter.c
  test_native_matrix.c
  test_main.c
  test_path_stream.c
  test_parser.c
  test_process.c
  test_source_manager.c
  test_types.c
  test_support.c
  test_unicode.c
  test_utf8.c
  ../fuzz/semantic_oracle.c
)

target_link_libraries(ruda_c_unit PRIVATE ruda_bootstrap)
target_include_directories(ruda_c_unit PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../fuzz"
)
target_compile_definitions(ruda_c_unit PRIVATE
  RUDA_TEST_C_COMPILER="${CMAKE_C_COMPILER}"
  RUDA_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
if(NOT MSVC AND (RUDA_ENABLE_ADDRESS_SANITIZER OR RUDA_ENABLE_UNDEFINED_SANITIZER))
  set(ruda_test_native_sanitizers "")
  if(RUDA_ENABLE_ADDRESS_SANITIZER)
    list(APPEND ruda_test_native_sanitizers "address")
  endif()
  if(RUDA_ENABLE_UNDEFINED_SANITIZER)
    list(APPEND ruda_test_native_sanitizers "undefined")
  endif()
  list(JOIN ruda_test_native_sanitizers "," ruda_test_native_sanitizer_list)
  target_compile_definitions(ruda_c_unit PRIVATE
    RUDA_TEST_C_SANITIZER="-fsanitize=${ruda_test_native_sanitizer_list}"
  )
endif()
ruda_target_warnings(ruda_c_unit)
ruda_target_sanitizers(ruda_c_unit)
add_dependencies(ruda_c_unit ruda)

file(COPY snapshots DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../syntax" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../format" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/format-tree" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../../examples" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
configure_file(
  snapshots/version.json.in
  "${CMAKE_CURRENT_BINARY_DIR}/snapshots/version.json"
  @ONLY
)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/../../README.md"
  "${CMAKE_CURRENT_BINARY_DIR}/README.fixture"
  COPYONLY
)

add_test(NAME ruda_c_unit COMMAND ruda_c_unit "$<TARGET_FILE:ruda>")
set_tests_properties(ruda_c_unit PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
