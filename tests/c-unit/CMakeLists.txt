add_executable(ruda_c_unit
  test_allocator.c
  test_ast.c
  test_base.c
  test_bytecode.c
  test_checker.c
  test_cli.c
  test_diagnostic.c
  test_directory.c
  test_doc_examples.c
  test_document.c
  test_file.c
  test_formatter.c
  test_hash_map.c
  test_lexer.c
  test_manifest.c
  test_mir.c
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
