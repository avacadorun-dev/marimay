set_source_files_properties(runtime_profile_probe.c PROPERTIES
  HEADER_FILE_ONLY TRUE
)

add_executable(mmay_c_unit
  runtime_profile_probe.c
  test_allocator.c
  test_ast.c
  test_base.c
  test_checked_integer.c
  test_integer_range.c
  test_bytecode.c
  test_bytecode_support.c
  test_bytecode_model.c
  test_bytecode_program.c
  test_bytecode_program_nominal.c
  test_bytecode_verify_provenance.c
  test_bytecode_verify_nominal_matrix.c
  test_bytecode_imported_arrays.c
  test_bytecode_imported_results.c
  test_bytecode_vm.c
  test_bytecode_vm_support.c
  test_bytecode_vm_scalar.c
  test_bytecode_vm_arithmetic.c
  test_bytecode_vm_arrays.c
  test_bytecode_vm_array_storage.c
  test_bytecode_vm_array_mutation.c
  test_bytecode_vm_interface_support.c
  test_bytecode_vm_interfaces.c
  test_bytecode_vm_interface_values.c
  test_bytecode_vm_nominal.c
  test_bytecode_vm_nominal_boundary.c
  test_bytecode_vm_nominal_identity.c
  test_checker.c
  test_checker_support.c
  test_checker_core.c
  test_checker_records.c
  test_checker_enums.c
  test_checker_interfaces.c
  test_checker_imports.c
  test_checker_imports_determinism.c
  test_checker_generics.c
  test_checker_generics_imports.c
  test_checker_imports_support.c
  test_checker_imported_arrays.c
  test_imported_arrays_support.c
  test_checker_imported_results.c
  test_imported_results_support.c
  test_checker_nested_patterns.c
  test_checker_enum_match.c
  test_checker_match_scalar.c
  test_checker_match_control.c
  test_checker_calls_constants.c
  test_checker_operators.c
  test_checker_numeric_policy.c
  test_checker_control.c
  test_checker_failures.c
  test_checker_arrays.c
  test_checker_array_mutation.c
  test_checker_array_types.c
  test_mir_array_mutation.c
  test_checker_facts.c
  test_checker_resolution.c
  test_receiver_collection_names.c
  test_receiver_collection_service.c
  test_check_entry.c
  test_cli.c
  test_cli_build.c
  test_cli_build_profiles.c
  test_cli_check.c
  test_cli_heap.c
  test_cli_run.c
  test_cli_test.c
  test_cli_cp14_imported_arrays.c
  test_cli_cp15_imported_results.c
  test_cli_cp16_receiver_names.c
  test_cli_cp17_array_mutation.c
  test_cli_cp18_array_spelling.c
  test_cli_cp20_algorithm_corpus.c
  test_compiler_service.c
  test_compiler_service_array_types.c
  test_compiler_service_incremental.c
  test_compiler_service_queries.c
  test_cp9_io_pipeline.c
  test_cp11_test_pipeline.c
  test_diagnostic.c
  test_defer_cp7.c
  test_directory.c
  test_doc_examples.c
  test_document.c
  test_file.c
  test_final_preview.c
  test_formatter.c
  test_formatter_support.c
  test_formatter_core.c
  test_formatter_array_types.c
  test_formatter_trivia.c
  test_formatter_failures.c
  test_hash_map.c
  test_lexer.c
  test_manifest.c
  test_managed.c
  runtime/test_managed_resource.c
  runtime/test_managed_array_access.c
  runtime/test_managed_array_capacity.c
  runtime/test_managed_array_nested.c
  runtime/test_managed_array_storage.c
  runtime/test_managed_record_view.c
  runtime/test_managed_mutation.c
  test_managed_objects.c
  test_number_format.c
  test_mir_calls.c
  test_mir.c
  test_mir_arrays.c
  test_mir_arrays_rhs.c
  test_mir_range.c
  test_mir_integer_range.c
  test_mir_imported_arrays.c
  test_mir_imported_results.c
  test_mir_bytecode.c
  test_mir_bytecode_model.c
  test_mir_bytecode_programs.c
  test_mir_bytecode_support.c
  test_mir_enums.c
  test_mir_interface_execution.c
  test_mir_interfaces.c
  test_mir_records.c
  test_mir_scalar.c
  test_mir_support.c
  test_native_c_emitter.c
  test_native_c_emitter_arrays.c
  test_native_c_emitter_array_access.c
  test_native_c_emitter_array_storage.c
  test_native_c_emitter_managed_views.c
  test_native_c_emitter_array_mutation.c
  test_native_c_emitter_baseline.c
  test_native_c_emitter_matrix.c
  test_native_c_emitter_nominal.c
  test_native_c_emitter_generics.c
  test_native_c_emitter_nominal_differential.c
  test_native_c_emitter_imports.c
  test_native_c_emitter_imported_arrays.c
  test_native_c_emitter_imported_results.c
  test_native_c_emitter_scalar.c
  test_native_c_emitter_strings.c
  test_native_c_emitter_support.c
  test_native_matrix.c
  test_nominal_engine_allocations.c
  test_numeric_cp6_differential.c
  test_main.c
  test_path_stream.c
  test_parser.c
  test_parser_support.c
  test_parser_core.c
  test_parser_declarations.c
  test_parser_expressions.c
  test_parser_nested_generics.c
  test_parser_array_types.c
  test_parser_incremental.c
  test_process.c
  test_runtime_io.c
  test_source_manager.c
  test_types.c
  test_support.c
  test_unicode.c
  test_utf8.c
  ../fuzz/semantic_oracle.c
)

target_link_libraries(mmay_c_unit PRIVATE mmay_bootstrap)
target_include_directories(mmay_c_unit PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../fuzz"
)
target_compile_definitions(mmay_c_unit PRIVATE
  MMAY_TEST_C_COMPILER="${CMAKE_C_COMPILER}"
  MMAY_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  MMAY_TEST_NATIVE_RUNTIME_ARCHIVE="$<TARGET_FILE:mmay_native_runtime_debug>"
  MMAY_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE="$<TARGET_FILE:mmay_native_runtime_debug>"
  MMAY_TEST_NATIVE_RUNTIME_RELEASE_ARCHIVE="$<TARGET_FILE:mmay_native_runtime_release>"
  MMAY_TEST_NATIVE_RUNTIME_DEBUG_MANIFEST="${MMAY_NATIVE_RUNTIME_DEBUG_MANIFEST}"
  MMAY_TEST_NATIVE_RUNTIME_RELEASE_MANIFEST="${MMAY_NATIVE_RUNTIME_RELEASE_MANIFEST}"
  MMAY_TEST_NATIVE_BOOTSTRAP_INCLUDE="${CMAKE_SOURCE_DIR}/bootstrap/include"
  MMAY_TEST_NATIVE_RUNTIME_INCLUDE="${CMAKE_SOURCE_DIR}/runtime/include"
)
if(MMAY_NATIVE_C_SANITIZER_FLAG)
  target_compile_definitions(mmay_c_unit PRIVATE
    MMAY_TEST_C_SANITIZER="${MMAY_NATIVE_C_SANITIZER_FLAG}"
  )
endif()
mmay_target_warnings(mmay_c_unit)
mmay_target_sanitizers(mmay_c_unit)
add_dependencies(mmay_c_unit
  mmay
  mmay_native_runtime_debug
  mmay_native_runtime_release
)

file(COPY snapshots DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../syntax" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../format" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/format-tree" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../../examples" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/check-tree" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-tree" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-mismatch" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-private" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-alloc" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-invalid-expect" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/check-invalid-manifest" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/check-invalid-identity" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/check-invalid-identity-nul" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../run" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../preview/checkpoint12" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp14-imported-arrays" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp14-imported-arrays-private" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp14-imported-arrays-bounds" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp15-imported-results" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp15-imported-results-private" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp15-imported-results-nonexhaustive" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp16-receiver-names" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp17-array-mutation" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp17-array-iterator-invalidated" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp18-array-spelling" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp18-array-spelling-bounds" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp19-resource-limits" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp20-algorithm-corpus" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY
  "${CMAKE_CURRENT_SOURCE_DIR}/check-bad.my"
  "${CMAKE_CURRENT_SOURCE_DIR}/check-incomplete.my"
  DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
)
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

add_test(NAME mmay_c_unit COMMAND mmay_c_unit "$<TARGET_FILE:mmay>")
set_tests_properties(mmay_c_unit PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_test(
  NAME mmay_install_relocation
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_BINARY_DIR=${CMAKE_BINARY_DIR}"
    "-DMMAY_TEST_CONFIG=$<CONFIG>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/run/cp9/acceptance.my"
    "-DMMAY_TEST_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
    "-DMMAY_TEST_RUNTIME_VERSION=${MMAY_RUNTIME_VERSION}"
    "-DMMAY_TEST_RELEASE_ARCHIVE=${MMAY_NATIVE_RUNTIME_RELEASE_FILENAME}"
    "-DMMAY_TEST_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_install_relocation.cmake"
)
