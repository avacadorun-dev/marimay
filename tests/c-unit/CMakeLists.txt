set_source_files_properties(runtime_profile_probe.c PROPERTIES
  HEADER_FILE_ONLY TRUE
)

set(MMAY_C_UNIT_SHARD_COUNT 16)

add_executable(mmay_cp32_process_fixture cp32_process_fixture.c)
mmay_target_warnings(mmay_cp32_process_fixture)
if(UNIX)
  add_executable(
    mmay_cp32_broken_pipe_launcher
    cp32_broken_pipe_launcher.c
  )
  mmay_target_warnings(mmay_cp32_broken_pipe_launcher)
endif()

add_executable(mmay_c_unit
  runtime_profile_probe.c
  test_allocator.c
  test_ast.c
  test_base.c
  test_checked_integer.c
  test_integer_range.c
  test_integer_text.c
  test_string_interpolation.c
  test_bytecode.c
  test_bytecode_support.c
  test_bytecode_model.c
  test_cp30_bytecode.c
  test_cp30_checker.c
  test_cp30_mir.c
  test_cp31_program_arguments.c
  test_cp32_streams.c
  test_cp33_text_bytes.c
  test_native_text_failures.c
  test_cp34_fs.c
  test_cp34_fs_pipeline.c
  test_cp35_environment.c
  test_cp35_context_pipeline.c
  test_cp36_collections.c
  test_collection_cursor.c
  test_collection_native_failures.c
  test_cp37_cli_parser.c
  test_cp38_json.c
  test_checker_json.c
  test_bytecode_program.c
  test_bytecode_program_nominal.c
  test_bytecode_verify_provenance.c
  test_bytecode_verify_nominal_matrix.c
  test_bytecode_imported_arrays.c
  test_bytecode_layout_features.c
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
  test_checker_record_imports.c
  test_checker_imports_determinism.c
  test_checker_generics.c
  test_checker_generic_lifetime.c
  test_checker_generics_imports.c
  test_checker_map_error_imports.c
  test_checker_map_error_context.c
  test_checker_mapper_values.c
  test_checker_enum_bottom.c
  test_checker_enum_pattern_names.c
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
  test_checker_equality.c
  test_checker_expectations.c
  test_native_test_helpers.c
  test_checker_expression_statements.c
  test_checker_void_match.c
  test_checker_operators.c
  test_checker_numeric_policy.c
  test_checker_control.c
  test_checker_failures.c
  test_checker_arrays.c
  test_checker_array_mutation.c
  test_checker_array_edit.c
  test_checker_array_search.c
  test_checker_function_values.c
  test_checker_option_condition.c
  test_checker_array_types.c
  test_mir_array_mutation.c
  test_checker_facts.c
  test_checker_resolution.c
  test_receiver_collection_names.c
  test_receiver_collection_service.c
  test_check_entry.c
  test_cli.c
  test_cli_release_metadata.c
  test_cli_build.c
  test_cli_build_profiles.c
  test_cli_check.c
  test_cli_heap.c
  test_cli_run.c
  test_cli_test.c
  test_cli_test_bytecode.c
  test_cli_test_native.c
  test_cli_test_locations.c
  test_cli_cp14_imported_arrays.c
  test_cli_cp15_imported_results.c
  test_cli_cp16_receiver_names.c
  test_cli_cp17_array_mutation.c
  test_cli_cp18_array_spelling.c
  test_cli_cp20_algorithm_corpus.c
  test_cli_cp39.c
  test_compiler_service.c
  test_compiler_service_array_types.c
  test_compiler_service_incremental.c
  test_compiler_service_queries.c
  test_compiler_service_intrinsics.c
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
  test_formatter_boolean_groups.c
  test_formatter_braced_groups.c
  test_formatter_trivia.c
  test_formatter_failures.c
  test_hash_map.c
  test_lexer.c
  test_manifest.c
  test_managed.c
  runtime/test_managed_resource.c
  runtime/test_managed_string.c
  runtime/test_managed_array_access.c
  runtime/test_managed_array_capacity.c
  runtime/test_managed_array_nested.c
  runtime/test_managed_array_storage.c
  runtime/test_managed_array_edit.c
  runtime/test_managed_record_view.c
  runtime/test_managed_mutation.c
  runtime/test_managed_map_set.c
  runtime/test_managed_map_probe.c
  runtime/test_managed_collection_iteration.c
  runtime/test_managed_map_storage.c
  runtime/test_managed_collection_churn.c
  runtime/test_managed_map_cache.c
  runtime/test_managed_set_insertion.c
  runtime/test_managed_reference_validation.c
  runtime/test_program_arguments.c
  test_managed_objects.c
  test_number_format.c
  test_mir_calls.c
  test_mir.c
  test_mir_arrays.c
  test_mir_arrays_rhs.c
  test_mir_range.c
  test_mir_integer_range.c
  test_mir_range_convergence.c
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
  test_native_c_emitter_test_entry.c
  test_native_c_emitter_arrays.c
  test_native_c_emitter_array_access.c
  test_native_c_emitter_array_storage.c
  test_native_c_emitter_roots.c
  test_native_c_emitter_view_lifetimes.c
  test_native_c_emitter_managed_views.c
  test_native_c_emitter_call_policy.c
  test_native_c_emitter_relations.c
  test_native_c_emitter_array_mutation.c
  test_array_edit_engines.c
  test_function_value_engines.c
  test_callable_engine_support.c
  test_array_search_engines.c
  test_mapper_value_engines.c
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
  test_pre1_surface_differential.c
  test_main.c
  test_path_stream.c
  test_parser.c
  test_parser_support.c
  test_parser_core.c
  test_parser_declarations.c
  test_parser_expressions.c
  test_parser_for_recovery.c
  test_parser_option_condition.c
  test_parser_nested_generics.c
  test_parser_array_types.c
  test_parser_incremental.c
  test_process.c
  test_runtime_lines.c
  test_file_line_engines.c
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
  "${CMAKE_SOURCE_DIR}/bootstrap/src/cli"
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
  MMAY_TEST_CP32_PROCESS_FIXTURE="$<TARGET_FILE:mmay_cp32_process_fixture>"
  MMAY_TEST_C_UNIT_SHARD_COUNT=${MMAY_C_UNIT_SHARD_COUNT}
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
  mmay_cp32_process_fixture
)
if(UNIX)
  add_dependencies(mmay_c_unit mmay_cp32_broken_pipe_launcher)
endif()

set(MMAY_C_UNIT_FIXTURE_DIRECTORIES
  "${CMAKE_CURRENT_SOURCE_DIR}/snapshots"
  "${CMAKE_CURRENT_SOURCE_DIR}/../syntax"
  "${CMAKE_CURRENT_SOURCE_DIR}/../format"
  "${CMAKE_CURRENT_SOURCE_DIR}/format-tree"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../examples"
  "${CMAKE_CURRENT_SOURCE_DIR}/check-tree"
  "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-tree"
  "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-mismatch"
  "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-prepared"
  "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-native"
  "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-private"
  "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-alloc"
  "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint11-invalid-expect"
  "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint31-args"
  "${CMAKE_CURRENT_SOURCE_DIR}/check-invalid-manifest"
  "${CMAKE_CURRENT_SOURCE_DIR}/check-invalid-identity"
  "${CMAKE_CURRENT_SOURCE_DIR}/check-invalid-identity-nul"
  "${CMAKE_CURRENT_SOURCE_DIR}/../run"
  "${CMAKE_CURRENT_SOURCE_DIR}/../preview/checkpoint12"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp14-imported-arrays"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp14-imported-arrays-private"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp14-imported-arrays-bounds"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp15-imported-results"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp15-imported-results-private"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp15-imported-results-nonexhaustive"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp16-receiver-names"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp17-array-mutation"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp17-array-iterator-invalidated"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp18-array-spelling"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp18-array-spelling-bounds"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp19-resource-limits"
  "${CMAKE_CURRENT_SOURCE_DIR}/../post-preview/cp20-algorithm-corpus"
)

function(mmay_prepare_c_unit_working_directory destination)
  file(MAKE_DIRECTORY "${destination}")
  foreach(fixture_directory IN LISTS MMAY_C_UNIT_FIXTURE_DIRECTORIES)
    file(COPY "${fixture_directory}" DESTINATION "${destination}")
  endforeach()
  # Tool-owned state is generated for the traversal test rather than shipped
  # as a tracked source-distribution member.
  file(MAKE_DIRECTORY "${destination}/format-tree/.mmay")
  file(COPY
    "${CMAKE_CURRENT_SOURCE_DIR}/check-bad.my"
    "${CMAKE_CURRENT_SOURCE_DIR}/check-incomplete.my"
    DESTINATION "${destination}"
  )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/snapshots/version.json.in"
    "${destination}/snapshots/version.json"
    @ONLY
  )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/snapshots/version.txt.in"
    "${destination}/snapshots/version.txt"
    @ONLY
  )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../README.md"
    "${destination}/README.fixture"
    COPYONLY
  )
endfunction()

mmay_prepare_c_unit_working_directory("${CMAKE_CURRENT_BINARY_DIR}")

math(EXPR MMAY_C_UNIT_LAST_SHARD "${MMAY_C_UNIT_SHARD_COUNT} - 1")
set(MMAY_C_UNIT_SHARD_NAMES
  frontend
  checker
  bytecode
  runtime
  native
  cp31
  final-preview
  cli-core
  cli-build
  cli-check
  cli-heap
  cli-run
  cli-test
  cp14-cp15
  cp16-cp17
  cp18-cp20
)
set(MMAY_C_UNIT_SHARD_COSTS
  2 80 25 70 440 640 480 1 225 75 330 265 375 420 505 625
)
foreach(shard RANGE 0 ${MMAY_C_UNIT_LAST_SHARD})
  list(GET MMAY_C_UNIT_SHARD_NAMES ${shard} shard_name)
  list(GET MMAY_C_UNIT_SHARD_COSTS ${shard} shard_cost)
  set(test_name "mmay_c_unit_${shard_name}")
  set(test_directory "${CMAKE_CURRENT_BINARY_DIR}/suite-${shard}")
  mmay_prepare_c_unit_working_directory("${test_directory}")
  add_test(
    NAME "${test_name}"
    COMMAND mmay_c_unit "$<TARGET_FILE:mmay>" --suite-shard "${shard}"
  )
  set_tests_properties(
    "${test_name}" PROPERTIES
    WORKING_DIRECTORY "${test_directory}"
    LABELS "c-unit;${shard_name}"
    COST "${shard_cost}"
    TIMEOUT 3600
  )
  add_custom_command(TARGET mmay_c_unit POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_FILE:mmay_c_unit>"
      "${test_directory}/$<TARGET_FILE_NAME:mmay_c_unit>"
    VERBATIM
  )
endforeach()

# The final-preview shard nests `mmay test --engine both`, compiler processes,
# and generated native test processes. Bound ASan's concurrent process fan-out
# without slowing the lower-overhead Debug, Release, or TSan schedules.
if(MMAY_ENABLE_ADDRESS_SANITIZER)
  set_tests_properties(
    mmay_c_unit_final-preview PROPERTIES PROCESSORS 2
  )
endif()

set(MMAY_CP31_ALLOCATION_SHARD_COUNT 8)
math(EXPR MMAY_CP31_ALLOCATION_LAST_SHARD
  "${MMAY_CP31_ALLOCATION_SHARD_COUNT} - 1"
)
foreach(shard RANGE 0 ${MMAY_CP31_ALLOCATION_LAST_SHARD})
  set(test_name "mmay_cp31_allocation_${shard}")
  add_test(
    NAME "${test_name}"
    COMMAND mmay_c_unit "$<TARGET_FILE:mmay>"
      --cp31-allocation-shard "${shard}"
      "${MMAY_CP31_ALLOCATION_SHARD_COUNT}"
  )
  set_tests_properties(
    "${test_name}" PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    LABELS "allocation"
    COST 70
  )
endforeach()
set_tests_properties(mmay_cp31_allocation_0 PROPERTIES COST 100)

function(mmay_add_focused_checkpoint_test checkpoint cost)
  set(test_name "mmay_${checkpoint}_unit")
  set(test_directory
    "${CMAKE_CURRENT_BINARY_DIR}/focused-${checkpoint}"
  )
  mmay_prepare_c_unit_working_directory("${test_directory}")
  add_test(
    NAME "${test_name}"
    COMMAND mmay_c_unit "$<TARGET_FILE:mmay>" "--${checkpoint}"
  )
  set_tests_properties(
    "${test_name}" PROPERTIES
    WORKING_DIRECTORY "${test_directory}"
    LABELS "c-unit;focused;${checkpoint}"
    COST "${cost}"
    TIMEOUT 3600
  )
  add_custom_command(TARGET mmay_c_unit POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_FILE:mmay_c_unit>"
      "${test_directory}/$<TARGET_FILE_NAME:mmay_c_unit>"
    VERBATIM
  )
endfunction()

# Each leaf test function has exactly one CTest owner. These named checkpoint
# owners contain the post-CP31 families that are intentionally absent from the
# broad suite shards above, so a full suite does not execute them twice.
mmay_add_focused_checkpoint_test(cp32 12)
mmay_add_focused_checkpoint_test(cp33 12)
mmay_add_focused_checkpoint_test(cp34 1)
mmay_add_focused_checkpoint_test(cp35 5)
mmay_add_focused_checkpoint_test(cp36 16)
mmay_add_focused_checkpoint_test(cp37 1)
mmay_add_focused_checkpoint_test(cp38 1)
mmay_add_focused_checkpoint_test(cp42 10)
mmay_add_focused_checkpoint_test(cp41 10)
mmay_add_focused_checkpoint_test(cp39 1)

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

add_test(
  NAME mmay_cp29_release_contract
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_BINARY_DIR=${CMAKE_BINARY_DIR}"
    "-DMMAY_TEST_CONFIG=$<CONFIG>"
    "-DMMAY_TEST_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_DATADIR=${CMAKE_INSTALL_DATADIR}"
    "-DMMAY_TEST_VERSION=${MMAY_COMPILER_VERSION}"
    "-DMMAY_TEST_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp29_release_contract.cmake"
)

if(WIN32)
  set(MMAY_TEST_CP39_ARCHIVE_FORMAT "zip")
else()
  set(MMAY_TEST_CP39_ARCHIVE_FORMAT "tar.gz")
endif()
string(TOLOWER "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
  MMAY_TEST_CP39_HOST)
add_test(
  NAME mmay_cp39_distribution
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_BINARY_DIR=${CMAKE_BINARY_DIR}"
    "-DMMAY_TEST_CONFIG=$<CONFIG>"
    "-DMMAY_TEST_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DMMAY_TEST_VERSION=${MMAY_COMPILER_VERSION}"
    "-DMMAY_TEST_HOST=${MMAY_TEST_CP39_HOST}"
    "-DMMAY_TEST_ARCHIVE_FORMAT=${MMAY_TEST_CP39_ARCHIVE_FORMAT}"
    "-DMMAY_TEST_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp39_distribution.cmake"
)

add_test(
  NAME mmay_cp39_packaging
  CONFIGURATIONS Release
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_BINARY_DIR=${CMAKE_BINARY_DIR}"
    "-DMMAY_TEST_CONFIG=$<CONFIG>"
    "-DMMAY_TEST_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DMMAY_TEST_VERSION=${MMAY_COMPILER_VERSION}"
    "-DMMAY_TEST_HOST=${MMAY_TEST_CP39_HOST}"
    "-DMMAY_TEST_ARCHIVE_FORMAT=${MMAY_TEST_CP39_ARCHIVE_FORMAT}"
    "-DMMAY_TEST_SOURCE_DATE_EPOCH=${MMAY_SOURCE_DATE_EPOCH}"
    "-DMMAY_TEST_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp39_packaging.cmake"
)

set(mmay_cp32_process_test_arguments
  "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
  "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp32-stdio"
  "-DMMAY_TEST_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/cp32-process-streams-native${CMAKE_EXECUTABLE_SUFFIX}"
)
if(UNIX)
  list(APPEND mmay_cp32_process_test_arguments
    "-DMMAY_TEST_BROKEN_PIPE_LAUNCHER=$<TARGET_FILE:mmay_cp32_broken_pipe_launcher>"
  )
endif()
add_test(
  NAME mmay_cp32_process_streams
  COMMAND "${CMAKE_COMMAND}"
    ${mmay_cp32_process_test_arguments}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp32_process_streams.cmake"
)

add_test(
  NAME mmay_cp33_process_text_bytes
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_BINARY_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp33-binary-stdio"
    "-DMMAY_TEST_ITERATOR_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp33-bytes-iterator-invalidated"
    "-DMMAY_TEST_BINARY_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/cp33-binary-native${CMAKE_EXECUTABLE_SUFFIX}"
    "-DMMAY_TEST_ITERATOR_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/cp33-iterator-native${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp33_process_text_bytes.cmake"
)

add_test(
  NAME mmay_cp34_process_files
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp34-files-io"
    "-DMMAY_TEST_CLEANUP_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp34-files-cleanup"
    "-DMMAY_TEST_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/cp34-files-native${CMAKE_EXECUTABLE_SUFFIX}"
    "-DMMAY_TEST_CLEANUP_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/cp34-files-cleanup-native${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp34_process_files.cmake"
)

add_test(
  NAME mmay_cp35_process_context
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_ENV_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp35-environment"
    "-DMMAY_TEST_PROCESS_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp35-process-context"
    "-DMMAY_TEST_ENV_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/cp35-environment-native${CMAKE_EXECUTABLE_SUFFIX}"
    "-DMMAY_TEST_PROCESS_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/cp35-process-native${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp35_process_context.cmake"
)

add_test(
  NAME mmay_cp36_map_set
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp36-map-set"
    "-DMMAY_TEST_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/cp36-map-set${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp36_map_set.cmake"
)

add_test(
  NAME mmay_cp37_cli_parser
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp37-cli-parser"
    "-DMMAY_TEST_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/cp37-cli-parser${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp37_cli_parser.cmake"
)

add_test(
  NAME mmay_cli_notes
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cli-notes"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cli_notes.cmake"
)

add_test(
  NAME mmay_cli_frequency
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cli-frequency"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cli_frequency.cmake"
)

add_test(
  NAME mmay_cli_config_migrate
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cli-config-migrate"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cli_config_migrate.cmake"
)

add_test(
  NAME mmay_record_imports
  COMMAND "$<TARGET_FILE:mmay>" test
    "${CMAKE_SOURCE_DIR}/tests/public-alpha/record-imports" --engine both
)
add_test(
  NAME mmay_match_void
  COMMAND "$<TARGET_FILE:mmay>" test
    "${CMAKE_SOURCE_DIR}/tests/public-alpha/match-void" --engine both
)
add_test(
  NAME mmay_generic_lifetime
  COMMAND "$<TARGET_FILE:mmay>" test
    "${CMAKE_SOURCE_DIR}/tests/public-alpha/generic-lifetime" --engine both
)
add_test(
  NAME mmay_map_error_imports
  COMMAND "$<TARGET_FILE:mmay>" test
    "${CMAKE_SOURCE_DIR}/tests/public-alpha/map-error-imports" --engine both
)
add_test(
  NAME mmay_enum_pattern_names
  COMMAND "$<TARGET_FILE:mmay>" test
    "${CMAKE_SOURCE_DIR}/tests/public-alpha/enum-pattern-names" --engine both
)
add_test(
  NAME mmay_cp33_text_bytes
  COMMAND "$<TARGET_FILE:mmay>" test
    "${CMAKE_SOURCE_DIR}/tests/public-alpha/cp33-text-bytes" --engine both
)
add_test(
  NAME mmay_match_format
  COMMAND "$<TARGET_FILE:mmay>" test
    "${CMAKE_SOURCE_DIR}/tests/public-alpha/match-format" --engine both
)
add_test(
  NAME mmay_run_startup
  COMMAND "$<TARGET_FILE:mmay>" test
    "${CMAKE_SOURCE_DIR}/tests/public-alpha/run-startup" --engine both
)
foreach(regression IN ITEMS record-imports match-void generic-lifetime map-error-imports enum-pattern-names cp33-text-bytes match-format run-startup)
  add_test(
    NAME "mmay_format_${regression}"
    COMMAND "$<TARGET_FILE:mmay>" fmt
      "${CMAKE_SOURCE_DIR}/tests/public-alpha/${regression}" --check
  )
endforeach()
add_test(
  NAME mmay_cli_notes_model
  COMMAND "$<TARGET_FILE:mmay>" test
    "${CMAKE_SOURCE_DIR}/tests/public-alpha/cli-notes" --engine both
)

add_test(
  NAME mmay_typed_json
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/typed-json"
    "-DMMAY_TEST_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/typed-json${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_typed_json.cmake"
)
set_tests_properties(mmay_typed_json PROPERTIES LABELS "differential;typed-json" TIMEOUT 1800)

add_test(
  NAME mmay_cli_local_execution
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/local-execution"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cli_local_execution.cmake"
)
set_tests_properties(mmay_cli_local_execution PROPERTIES
  LABELS "differential;resource;local-execution" TIMEOUT 1800)

add_test(
  NAME mmay_cp38_json
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp38-json"
    "-DMMAY_TEST_ARTIFACT=${CMAKE_CURRENT_BINARY_DIR}/cp38-json${CMAKE_EXECUTABLE_SUFFIX}"
    "-DMMAY_TEST_INPUT=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp38-json/input.json"
    "-DMMAY_TEST_EXPECTED=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp38-json/expected.json"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp38_json.cmake"
)

add_test(
  NAME mmay_cp42_void_match
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp42-void-match"
    "-DMMAY_TEST_WORK=${CMAKE_CURRENT_BINARY_DIR}/cp42-void-match"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_void_match.cmake"
)
set_tests_properties(mmay_cp42_void_match PROPERTIES
  LABELS "differential;cp42" TIMEOUT 1800)

add_test(
  NAME mmay_cp42_test_helpers
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp42-test-helpers"
    "-DMMAY_TEST_WORK=${CMAKE_CURRENT_BINARY_DIR}/cp42-test-helpers"
    "-DMMAY_TEST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_test_helpers.cmake"
)
set_tests_properties(mmay_cp42_test_helpers PROPERTIES
  LABELS "differential;cp42" TIMEOUT 1800)

add_test(
  NAME mmay_cp42_map_error_context
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp42-map-error-context"
    "-DMMAY_TEST_WORK=${CMAKE_CURRENT_BINARY_DIR}/cp42-map-error-context"
    "-DMMAY_TEST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_map_error_context.cmake"
)
set_tests_properties(mmay_cp42_map_error_context PROPERTIES
  LABELS "differential;cp42" TIMEOUT 1800)

add_test(
  NAME mmay_cp42_test_locations
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_WORK=${CMAKE_CURRENT_BINARY_DIR}/cp42-test-locations"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_test_locations.cmake"
)
set_tests_properties(mmay_cp42_test_locations PROPERTIES
  LABELS "differential;cp42" TIMEOUT 1800)

add_test(
  NAME mmay_cp42_for_recovery
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_WORK=${CMAKE_CURRENT_BINARY_DIR}/cp42-for-recovery"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_for_recovery.cmake"
)
set_tests_properties(mmay_cp42_for_recovery PROPERTIES
  LABELS "differential;cp42" TIMEOUT 300)

# The install contracts update CMake's build-tree install manifest. They may
# overlap all other isolated tests, but not each other.
set_tests_properties(
  mmay_install_relocation
  mmay_cp29_release_contract
  mmay_cp39_distribution
  mmay_cp39_packaging
  PROPERTIES RESOURCE_LOCK "mmay_install_manifest"
)
set_tests_properties(mmay_install_relocation PROPERTIES COST 320)
set_tests_properties(mmay_cp29_release_contract PROPERTIES COST 200)
set_tests_properties(
  mmay_cp39_distribution
  PROPERTIES PROCESSORS 2 COST 700 TIMEOUT 3600 LABELS "cp39;distribution"
)
set_tests_properties(
  mmay_cp39_packaging
  PROPERTIES
    PROCESSORS 2
    COST 680
    TIMEOUT 3600
    LABELS "cp39;distribution;packaging"
)
set_tests_properties(mmay_cp32_process_streams PROPERTIES COST 320)
set_tests_properties(mmay_cp33_process_text_bytes PROPERTIES COST 380)
set_tests_properties(mmay_cp34_process_files PROPERTIES COST 380)
set_tests_properties(mmay_cp35_process_context PROPERTIES COST 380)
set_tests_properties(mmay_cp36_map_set PROPERTIES COST 400)
add_test(
  NAME mmay_cp42_option_binding
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp42-option-binding"
    "-DMMAY_TEST_WORK=${CMAKE_CURRENT_BINARY_DIR}/cp42-option-binding"
    "-DMMAY_TEST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_option_binding.cmake"
)
set_tests_properties(mmay_cp42_option_binding PROPERTIES PROCESSORS 2 COST 300 TIMEOUT 1200)
add_test(
  NAME mmay_cp42_array_edit
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp42-array-edit"
    "-DMMAY_TEST_WORK=${CMAKE_CURRENT_BINARY_DIR}/cp42-array-edit"
    "-DMMAY_TEST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_array_edit.cmake"
)
set_tests_properties(mmay_cp42_array_edit PROPERTIES PROCESSORS 2 COST 300 TIMEOUT 1200)
add_test(
  NAME mmay_cp42_function_values
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp42-function-values"
    "-DMMAY_TEST_WORK=${CMAKE_CURRENT_BINARY_DIR}/cp42-function-values"
    "-DMMAY_TEST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_function_values.cmake"
)
set_tests_properties(mmay_cp42_function_values PROPERTIES PROCESSORS 2 COST 300 TIMEOUT 1200)

add_test(
  NAME mmay_cp42_mapper_values
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp42-mapper-values"
    "-DMMAY_TEST_WORK=${CMAKE_CURRENT_BINARY_DIR}/cp42-mapper-values"
    "-DMMAY_TEST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_mapper_values.cmake"
)
set_tests_properties(mmay_cp42_mapper_values PROPERTIES PROCESSORS 2 COST 300 TIMEOUT 1200)

# CP37 creates uniquely named artifacts, but launches several compiler and
# native-program subprocesses. Reserve capacity without serializing every
# unrelated CTest owner behind it.
set_tests_properties(
  mmay_cp37_cli_parser
  PROPERTIES PROCESSORS 2 COST 720 TIMEOUT 1200
)
set_tests_properties(
  mmay_cp38_json
  # This end-to-end contract recompiles and runs the package several times.
  # Sanitizer instrumentation can push a healthy run past twenty minutes.
  PROPERTIES PROCESSORS 2 COST 1500 TIMEOUT 3600
)
if(MMAY_ENABLE_ADDRESS_SANITIZER)
  # The instrumented nested compiler has enough peak working set to be killed
  # externally when it overlaps the heaviest C-unit shards under parallel CTest.
  set_tests_properties(mmay_cp38_json PROPERTIES RUN_SERIAL TRUE)
  # Once the serial JSON owner releases the pool, overlap the longest remaining
  # native owner with the two-worker CP37/CP39 chain instead of leaving it in
  # the sanitizer tail behind allocation shards.
  set_tests_properties(mmay_c_unit_native PROPERTIES COST 1000)
endif()

add_test(
  NAME mmay_cp42_array_search
  COMMAND "${CMAKE_COMMAND}"
    "-DMMAY_TEST_TOOL=$<TARGET_FILE:mmay>"
    "-DMMAY_TEST_SOURCE=${CMAKE_SOURCE_DIR}/tests/public-alpha/cp42-array-search"
    "-DMMAY_TEST_WORK=${CMAKE_BINARY_DIR}/cp42-array-search"
    "-DMMAY_TEST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_array_search.cmake"
)
set_tests_properties(mmay_cp42_array_search PROPERTIES PROCESSORS 2 COST 300 TIMEOUT 1200)

add_test(NAME mmay_cp42_file_lines
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_file_lines.py"
    "$<TARGET_FILE:mmay>" "${CMAKE_SOURCE_DIR}/tests/public-alpha/cp42-file-lines"
    "${CMAKE_CURRENT_BINARY_DIR}/cp42-file-lines")
set_tests_properties(mmay_cp42_file_lines PROPERTIES PROCESSORS 2 COST 300 TIMEOUT 1200
  LABELS "differential;cp42")

add_test(NAME mmay_cp42_enum_bottom
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_cp42_enum_bottom.py"
    "$<TARGET_FILE:mmay>" "${CMAKE_SOURCE_DIR}/tests/public-alpha/cp42-enum-bottom"
    "${CMAKE_CURRENT_BINARY_DIR}/cp42-enum-bottom")
set_tests_properties(mmay_cp42_enum_bottom PROPERTIES PROCESSORS 2 COST 300 TIMEOUT 1200
  LABELS "differential;cp42")
