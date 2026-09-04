cmake_minimum_required(VERSION 3.25)

project(Marimay VERSION 0.1.0 LANGUAGES C)

include(CTest)
include(GNUInstallDirs)
include(cmake/MarimayWarnings.cmake)
include(cmake/MarimaySanitizers.cmake)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MMAY_WARNINGS_AS_ERRORS "Treat warnings in Marimay project code as errors" ON)
option(MMAY_ENABLE_ADDRESS_SANITIZER "Enable AddressSanitizer" OFF)
option(MMAY_ENABLE_UNDEFINED_SANITIZER "Enable UndefinedBehaviorSanitizer" OFF)
option(MMAY_ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer" OFF)
option(MMAY_BUILD_FUZZERS "Build Clang libFuzzer frontend targets" OFF)

if(MMAY_BUILD_FUZZERS)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Marimay libFuzzer targets require Clang")
  endif()
  include(CheckCSourceCompiles)
  set(MMAY_SAVED_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fsanitize=fuzzer")
  check_c_source_compiles(
    "#include <stddef.h>
     int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
       return data == NULL && size != 0;
     }"
    MMAY_COMPILER_HAS_LIBFUZZER
  )
  set(CMAKE_REQUIRED_FLAGS "${MMAY_SAVED_REQUIRED_FLAGS}")
  if(NOT MMAY_COMPILER_HAS_LIBFUZZER)
    message(FATAL_ERROR
      "MMAY_BUILD_FUZZERS requires a Clang distribution with the libFuzzer runtime"
    )
  endif()
endif()

if(WIN32)
  set(MMAY_PLATFORM_SOURCES
    runtime/platform/windows/directory.c
    runtime/platform/windows/environment.c
    runtime/platform/windows/file.c
    runtime/platform/windows/program_arguments.c
    runtime/platform/windows/process.c
  )
  set(MMAY_NATIVE_PROGRAM_ARGUMENTS_SOURCE
    runtime/platform/windows/program_arguments.c
  )
  set(MMAY_NATIVE_FS_SOURCES
    runtime/platform/windows/directory.c
    runtime/platform/windows/file.c
  )
  set(MMAY_NATIVE_CONTEXT_SOURCES
    runtime/platform/windows/environment.c
    runtime/platform/windows/process.c
  )
elseif(UNIX)
  set(MMAY_PLATFORM_SOURCES
    runtime/platform/posix/directory.c
    runtime/platform/posix/environment.c
    runtime/platform/posix/file.c
    runtime/platform/posix/program_arguments.c
    runtime/platform/posix/process.c
  )
  set(MMAY_NATIVE_PROGRAM_ARGUMENTS_SOURCE
    runtime/platform/posix/program_arguments.c
  )
  set(MMAY_NATIVE_FS_SOURCES
    runtime/platform/posix/directory.c
    runtime/platform/posix/file.c
  )
  set(MMAY_NATIVE_CONTEXT_SOURCES
    runtime/platform/posix/environment.c
    runtime/platform/posix/process.c
  )
else()
  message(FATAL_ERROR "No Stage 0 platform adapter for this host")
endif()

set(MMAY_COMPILER_VERSION "0.1.0-alpha.1")
set(MMAY_RUNTIME_VERSION "0.1.0-alpha.1")
set(MMAY_LANGUAGE_VERSION "0.1-design")
set(MMAY_BYTECODE_VERSION "unavailable")
set(MMAY_BYTECODE_COMPATIBILITY "none")
set(MMAY_STANDARD_LIBRARY_VERSION "0.1.0-alpha.1")
set(MMAY_STANDARD_LIBRARY_SCOPE "bounded-preview")
set(MMAY_STANDARD_LIBRARY_COMPATIBILITY "alpha-source")
set(MMAY_VERSION_SCHEMA "marimay.version/2")
set(MMAY_RELEASE_CHANNEL "private-preview")
set(MMAY_RELEASE_CLAIM "local-checker-backed-slice")
set(MMAY_RELEASE_PUBLICATION_READY_JSON "false")
set(MMAY_AVAILABLE_COMMANDS_JSON
  "[\"build\",\"check\",\"doctor\",\"fmt\",\"help\",\"init\",\"run\",\"test\",\"version\"]"
)
set(MMAY_STAGED_COMMANDS_JSON
  "[\"add\",\"audit\",\"bench\",\"doc\",\"exec\",\"explain\",\"fix\",\"fuzz\",\"generate\",\"list\",\"outdated\",\"remove\",\"update\",\"vendor\",\"vet\",\"why\"]"
)
set(MMAY_BOOTSTRAP_STAGE 0)

execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RESULT_VARIABLE MMAY_GIT_REVISION_RESULT
  OUTPUT_VARIABLE MMAY_SOURCE_REVISION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
execute_process(
  COMMAND git show -s --format=%ct HEAD
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RESULT_VARIABLE MMAY_GIT_EPOCH_RESULT
  OUTPUT_VARIABLE MMAY_SOURCE_DATE_EPOCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env TZ=UTC git show -s
    --date=format-local:%Y-%m-%dT%H:%M:%SZ --format=%cd HEAD
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RESULT_VARIABLE MMAY_GIT_DATE_RESULT
  OUTPUT_VARIABLE MMAY_SOURCE_DATE_ISO
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT MMAY_GIT_REVISION_RESULT EQUAL 0)
  set(MMAY_SOURCE_REVISION "unknown")
endif()
set(MMAY_SOURCE_DATE_FROM_ENV FALSE)
if(DEFINED ENV{SOURCE_DATE_EPOCH} AND NOT "$ENV{SOURCE_DATE_EPOCH}" STREQUAL "")
  set(MMAY_SOURCE_DATE_FROM_ENV TRUE)
  set(MMAY_SOURCE_DATE_EPOCH "$ENV{SOURCE_DATE_EPOCH}")
  string(TIMESTAMP MMAY_SOURCE_DATE_ISO "%Y-%m-%dT%H:%M:%SZ" UTC)
elseif(NOT MMAY_GIT_EPOCH_RESULT EQUAL 0)
  set(MMAY_SOURCE_DATE_EPOCH 0)
endif()
if(NOT MMAY_SOURCE_DATE_EPOCH MATCHES "^[0-9]+$")
  message(FATAL_ERROR "SOURCE_DATE_EPOCH must be a nonnegative decimal integer")
endif()
if(NOT MMAY_SOURCE_DATE_FROM_ENV AND NOT MMAY_GIT_DATE_RESULT EQUAL 0)
  set(MMAY_SOURCE_DATE_ISO "1970-01-01T00:00:00Z")
endif()

set(MMAY_NATIVE_RUNTIME_DEBUG_DIRECTORY
  "${CMAKE_CURRENT_BINARY_DIR}/native-runtime/debug"
)
set(MMAY_NATIVE_RUNTIME_RELEASE_DIRECTORY
  "${CMAKE_CURRENT_BINARY_DIR}/native-runtime/release"
)
set(MMAY_NATIVE_RUNTIME_DEBUG_ARCHIVE
  "${MMAY_NATIVE_RUNTIME_DEBUG_DIRECTORY}/${CMAKE_STATIC_LIBRARY_PREFIX}mmay_native_runtime_debug${CMAKE_STATIC_LIBRARY_SUFFIX}"
)
set(MMAY_NATIVE_RUNTIME_RELEASE_ARCHIVE
  "${MMAY_NATIVE_RUNTIME_RELEASE_DIRECTORY}/${CMAKE_STATIC_LIBRARY_PREFIX}mmay_native_runtime_release${CMAKE_STATIC_LIBRARY_SUFFIX}"
)
set(MMAY_NATIVE_RUNTIME_DEBUG_MANIFEST
  "${MMAY_NATIVE_RUNTIME_DEBUG_DIRECTORY}/mmay-native-runtime.manifest"
)
set(MMAY_NATIVE_RUNTIME_RELEASE_MANIFEST
  "${MMAY_NATIVE_RUNTIME_RELEASE_DIRECTORY}/mmay-native-runtime.manifest"
)
set(MMAY_NATIVE_INSTALLED_RUNTIME_RELATIVE
  "../${CMAKE_INSTALL_LIBDIR}/mmay/${MMAY_RUNTIME_VERSION}/native"
)
set(MMAY_NATIVE_INSTALLED_INCLUDE_RELATIVE
  "../${CMAKE_INSTALL_INCLUDEDIR}"
)
set(MMAY_NATIVE_RUNTIME_DEBUG_FILENAME
  "${CMAKE_STATIC_LIBRARY_PREFIX}mmay_native_runtime_debug${CMAKE_STATIC_LIBRARY_SUFFIX}"
)
set(MMAY_NATIVE_RUNTIME_RELEASE_FILENAME
  "${CMAKE_STATIC_LIBRARY_PREFIX}mmay_native_runtime_release${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

set(MMAY_NATIVE_C_SANITIZER_FLAG "")
if(MSVC AND MMAY_ENABLE_ADDRESS_SANITIZER)
  set(MMAY_NATIVE_C_SANITIZER_FLAG "/fsanitize=address")
elseif(NOT MSVC)
  set(mmay_native_c_sanitizers "")
  if(MMAY_ENABLE_ADDRESS_SANITIZER)
    list(APPEND mmay_native_c_sanitizers "address")
  endif()
  if(MMAY_ENABLE_UNDEFINED_SANITIZER)
    list(APPEND mmay_native_c_sanitizers "undefined")
  endif()
  if(MMAY_ENABLE_THREAD_SANITIZER)
    list(APPEND mmay_native_c_sanitizers "thread")
  endif()
  if(mmay_native_c_sanitizers)
    list(JOIN mmay_native_c_sanitizers "," mmay_native_c_sanitizer_list)
    set(MMAY_NATIVE_C_SANITIZER_FLAG
      "-fsanitize=${mmay_native_c_sanitizer_list}"
    )
  endif()
endif()

configure_file(
  cmake/mmay_build_config.h.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/mmay/build_config.h"
  @ONLY
)
configure_file(
  cmake/mmay_release_metadata.json.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/mmay-release-metadata.json"
  @ONLY
)
configure_file(
  cmake/MarimayInstallMetadata.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/MarimayInstallMetadata.cmake"
  @ONLY
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/std/cli/cli.my"
  "${CMAKE_CURRENT_SOURCE_DIR}/std/cli/parse.my"
  "${CMAKE_CURRENT_SOURCE_DIR}/std/cli/help.my"
  "${CMAKE_CURRENT_SOURCE_DIR}/std/json/json.my"
  "${CMAKE_CURRENT_SOURCE_DIR}/std/json/decode.my"
  "${CMAKE_CURRENT_SOURCE_DIR}/std/json/encode.my"
)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/std/cli/cli.my"
  MMAY_STD_CLI_MODEL_SOURCE_HEX HEX
)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/std/cli/parse.my"
  MMAY_STD_CLI_PARSE_SOURCE_HEX HEX
)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/std/cli/help.my"
  MMAY_STD_CLI_HELP_SOURCE_HEX HEX
)
string(REGEX REPLACE "(..)" "0x\\1," MMAY_STD_CLI_MODEL_SOURCE_BYTES
  "${MMAY_STD_CLI_MODEL_SOURCE_HEX}"
)
string(REGEX REPLACE "(..)" "0x\\1," MMAY_STD_CLI_PARSE_SOURCE_BYTES
  "${MMAY_STD_CLI_PARSE_SOURCE_HEX}"
)
string(REGEX REPLACE "(..)" "0x\\1," MMAY_STD_CLI_HELP_SOURCE_BYTES
  "${MMAY_STD_CLI_HELP_SOURCE_HEX}"
)
configure_file(
  cmake/mmay_std_cli_source.h.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/mmay_std_cli_source.h"
  @ONLY
)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/std/json/json.my"
  MMAY_STD_JSON_MODEL_SOURCE_HEX HEX
)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/std/json/decode.my"
  MMAY_STD_JSON_DECODE_SOURCE_HEX HEX
)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/std/json/encode.my"
  MMAY_STD_JSON_ENCODE_SOURCE_HEX HEX
)
string(REGEX REPLACE "(..)" "0x\\1," MMAY_STD_JSON_MODEL_SOURCE_BYTES
  "${MMAY_STD_JSON_MODEL_SOURCE_HEX}"
)
string(REGEX REPLACE "(..)" "0x\\1," MMAY_STD_JSON_DECODE_SOURCE_BYTES
  "${MMAY_STD_JSON_DECODE_SOURCE_HEX}"
)
string(REGEX REPLACE "(..)" "0x\\1," MMAY_STD_JSON_ENCODE_SOURCE_BYTES
  "${MMAY_STD_JSON_ENCODE_SOURCE_HEX}"
)
configure_file(
  cmake/mmay_std_json_source.h.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/mmay_std_json_source.h"
  @ONLY
)

file(MAKE_DIRECTORY
  "${MMAY_NATIVE_RUNTIME_DEBUG_DIRECTORY}"
  "${MMAY_NATIVE_RUNTIME_RELEASE_DIRECTORY}"
)
set(MMAY_NATIVE_RUNTIME_MANIFEST_PROFILE debug)
configure_file(
  cmake/mmay_native_runtime.manifest.in
  "${MMAY_NATIVE_RUNTIME_DEBUG_MANIFEST}"
  @ONLY
)
set(MMAY_NATIVE_RUNTIME_MANIFEST_PROFILE release)
configure_file(
  cmake/mmay_native_runtime.manifest.in
  "${MMAY_NATIVE_RUNTIME_RELEASE_MANIFEST}"
  @ONLY
)

set(MMAY_NATIVE_RUNTIME_SOURCES
  bootstrap/src/base/allocator.c
  bootstrap/src/base/checked_i64.c
  bootstrap/src/base/checked_integer.c
  bootstrap/src/base/f64_runtime.c
  bootstrap/src/base/hash.c
  bootstrap/src/base/integer_text.c
  bootstrap/src/base/number_format.c
  bootstrap/src/base/owned_bytes.c
  bootstrap/src/base/path.c
  bootstrap/src/base/provenance.c
  bootstrap/src/base/text.c
  bootstrap/src/base/unicode.c
  bootstrap/src/base/utf8.c
  bootstrap/src/base/vector.c
  runtime/common/io_stdout.c
  runtime/common/io.c
  runtime/common/directory.c
  runtime/common/env.c
  runtime/common/process_context.c
  runtime/common/file.c
  runtime/common/fs.c
  runtime/common/managed.c
  runtime/common/managed_array_access.c
  runtime/common/managed_array_capacity.c
  runtime/common/managed_array_storage.c
  runtime/common/managed_resource.c
  runtime/common/managed_array_mutation.c
  runtime/common/managed_internal.c
  runtime/common/managed_string.c
  runtime/common/managed_record.c
  runtime/common/managed_enum.c
  runtime/common/managed_interface.c
  runtime/common/managed_map.c
  runtime/common/managed_set.c
  runtime/common/program_arguments.c
  runtime/common/profile.c
  runtime/common/test_output.c
  ${MMAY_NATIVE_PROGRAM_ARGUMENTS_SOURCE}
  ${MMAY_NATIVE_FS_SOURCES}
  ${MMAY_NATIVE_CONTEXT_SOURCES}
)

function(mmay_add_native_runtime target profile optimization output_directory)
  add_library(${target} STATIC ${MMAY_NATIVE_RUNTIME_SOURCES})
  target_include_directories(${target} PRIVATE
    bootstrap/include
    runtime/include
  )
  target_compile_definitions(${target} PRIVATE
    MMAY_RUNTIME_BUILD_PROFILE="${profile}"
    MMAY_RUNTIME_BUILD_VERSION="${MMAY_RUNTIME_VERSION}"
  )
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME "${target}"
    ARCHIVE_OUTPUT_DIRECTORY "${output_directory}"
  )
  foreach(mmay_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(${target} PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY_${mmay_config} "${output_directory}"
    )
  endforeach()
  mmay_target_sanitizers(${target})
  if(MSVC)
    target_compile_options(${target} PRIVATE /fp:strict "${optimization}")
  else()
    target_compile_options(${target} PRIVATE
      -fno-fast-math -frounding-math -ffp-contract=off
      -fexcess-precision=standard "${optimization}"
    )
  endif()
  mmay_target_warnings(${target})
endfunction()

if(MSVC)
  set(MMAY_NATIVE_RUNTIME_DEBUG_OPTIMIZATION /Od)
  set(MMAY_NATIVE_RUNTIME_RELEASE_OPTIMIZATION /O2)
else()
  set(MMAY_NATIVE_RUNTIME_DEBUG_OPTIMIZATION -O0)
  set(MMAY_NATIVE_RUNTIME_RELEASE_OPTIMIZATION -O2)
endif()
mmay_add_native_runtime(
  mmay_native_runtime_debug debug
  "${MMAY_NATIVE_RUNTIME_DEBUG_OPTIMIZATION}"
  "${MMAY_NATIVE_RUNTIME_DEBUG_DIRECTORY}"
)
mmay_add_native_runtime(
  mmay_native_runtime_release release
  "${MMAY_NATIVE_RUNTIME_RELEASE_OPTIMIZATION}"
  "${MMAY_NATIVE_RUNTIME_RELEASE_DIRECTORY}"
)

add_library(mmay_bootstrap STATIC
  bootstrap/src/ast/ast.c
  bootstrap/src/base/allocator.c
  bootstrap/src/base/arena.c
  bootstrap/src/base/checked_i64.c
  bootstrap/src/base/checked_integer.c
  bootstrap/src/base/f64.c
  bootstrap/src/base/f64_runtime.c
  bootstrap/src/base/hash.c
  bootstrap/src/base/integer_range.c
  bootstrap/src/base/integer_text.c
  bootstrap/src/base/map.c
  bootstrap/src/base/number_format.c
  bootstrap/src/base/owned_bytes.c
  bootstrap/src/base/path.c
  bootstrap/src/base/provenance.c
  bootstrap/src/base/stream.c
  bootstrap/src/base/string_literal.c
  bootstrap/src/base/text.c
  bootstrap/src/base/unicode.c
  bootstrap/src/base/utf8.c
  bootstrap/src/base/vector.c
  bootstrap/src/base/writer.c
  bootstrap/src/bytecode/bytecode.c
  bootstrap/src/bytecode/bytecode_arrays.c
  bootstrap/src/bytecode/bytecode_collections.c
  bootstrap/src/bytecode/bytecode_nominal.c
  bootstrap/src/bytecode/bytecode_codec.c
  bootstrap/src/bytecode/bytecode_codec_nominal.c
  bootstrap/src/bytecode/bytecode_codec_nominal_values.c
  bootstrap/src/bytecode/bytecode_verify.c
  bootstrap/src/bytecode/bytecode_verify_defer.c
  bootstrap/src/bytecode/bytecode_verify_instructions.c
  bootstrap/src/bytecode/bytecode_verify_terminator.c
  bootstrap/src/bytecode/bytecode_verify_format.c
  bootstrap/src/bytecode/bytecode_verify_parse.c
  bootstrap/src/bytecode/bytecode_verify_text.c
  bootstrap/src/bytecode/bytecode_verify_integer_range.c
  bootstrap/src/bytecode/bytecode_verify_io.c
  bootstrap/src/bytecode/bytecode_verify_fs.c
  bootstrap/src/bytecode/bytecode_verify_context.c
  bootstrap/src/bytecode/bytecode_verify_collections.c
  bootstrap/src/bytecode/bytecode_verify_nominal.c
  bootstrap/src/bytecode/bytecode_verify_numeric.c
  bootstrap/src/bytecode/bytecode_verify_provenance.c
  bootstrap/src/bytecode/program.c
  bootstrap/src/bytecode/program_limits.c
  bootstrap/src/bytecode/program_verify_nominal.c
  bootstrap/src/bytecode/mir_lower.c
  bootstrap/src/bytecode/mir_lower_context.c
  bootstrap/src/bytecode/mir_lower_format.c
  bootstrap/src/bytecode/mir_lower_fs.c
  bootstrap/src/bytecode/mir_lower_text.c
  bootstrap/src/bytecode/mir_lower_nominal.c
  bootstrap/src/bytecode/vm.c
  bootstrap/src/bytecode/vm_array.c
  bootstrap/src/bytecode/vm_range.c
  bootstrap/src/bytecode/vm_array_mutation.c
  bootstrap/src/bytecode/vm_collection.c
  bootstrap/src/bytecode/vm_defer.c
  bootstrap/src/bytecode/vm_interface.c
  bootstrap/src/bytecode/vm_io.c
  bootstrap/src/bytecode/vm_fs.c
  bootstrap/src/bytecode/vm_context.c
  bootstrap/src/bytecode/vm_format.c
  bootstrap/src/bytecode/vm_text.c
  bootstrap/src/bytecode/vm_parse.c
  bootstrap/src/bytecode/vm_nominal.c
  bootstrap/src/bytecode/vm_numeric.c
  bootstrap/src/bytecode/vm_prepare.c
  bootstrap/src/bytecode/vm_arguments.c
  bootstrap/src/bytecode/vm_value.c
  bootstrap/src/check/array.c
  bootstrap/src/check/checker_array_mutation.c
  bootstrap/src/check/checker_array_types.c
  bootstrap/src/check/checker.c
  bootstrap/src/check/checker_cfg.c
  bootstrap/src/check/checker_defer.c
  bootstrap/src/check/checker_core.c
  bootstrap/src/check/checker_compound_types.c
  bootstrap/src/check/checker_collection_types.c
  bootstrap/src/check/checker_collection_loop.c
  bootstrap/src/check/checker_expr_collections.c
  bootstrap/src/check/checker_declaration_constants.c
  bootstrap/src/check/checker_declaration_enums.c
  bootstrap/src/check/checker_derive.c
  bootstrap/src/check/checker_equality.c
  bootstrap/src/check/checker_declaration_functions.c
  bootstrap/src/check/checker_declaration_interfaces.c
  bootstrap/src/check/checker_declaration_lifecycle.c
  bootstrap/src/check/checker_declaration_nominal.c
  bootstrap/src/check/checker_declaration_publication_nominal.c
  bootstrap/src/check/checker_declaration_records.c
  bootstrap/src/check/checker_expr_calls.c
  bootstrap/src/check/checker_receiver_calls.c
  bootstrap/src/check/checker_expr_prelude.c
  bootstrap/src/check/checker_expr_try.c
  bootstrap/src/check/checker_expr_map_error.c
  bootstrap/src/check/checker_expr_numeric.c
  bootstrap/src/check/checker_expr_interfaces.c
  bootstrap/src/check/checker_expr_io.c
  bootstrap/src/check/checker_expr_fs.c
  bootstrap/src/check/checker_expr_context.c
  bootstrap/src/check/checker_expr_test.c
  bootstrap/src/check/checker_generic_types.c
  bootstrap/src/check/checker_generic_specialization.c
  bootstrap/src/check/checker_prelude.c
  bootstrap/src/check/checker_expr_nominal.c
  bootstrap/src/check/checker_expr_records.c
  bootstrap/src/check/checker_expr_scalar.c
  bootstrap/src/check/checker_expr_string.c
  bootstrap/src/check/checker_expr_text.c
  bootstrap/src/check/checker_expr_tuples.c
  bootstrap/src/check/checker_expression.c
  bootstrap/src/check/checker_interpret.c
  bootstrap/src/check/checker_imported_nominal.c
  bootstrap/src/check/checker_imported_enums.c
  bootstrap/src/check/checker_lookup.c
  bootstrap/src/check/checker_lookup_imports.c
  bootstrap/src/check/checker_match_analysis.c
  bootstrap/src/check/checker_match_binding.c
  bootstrap/src/check/checker_match_dispatch.c
  bootstrap/src/check/checker_match_lower.c
  bootstrap/src/check/checker_match_pattern.c
  bootstrap/src/check/checker_match_witness.c
  bootstrap/src/check/checker_publication.c
  bootstrap/src/check/checker_range_loop.c
  bootstrap/src/check/checker_statement.c
  bootstrap/src/check/checker_tuple_match.c
  bootstrap/src/check/entry.c
  bootstrap/src/check/facts.c
  bootstrap/src/check/package_graph.c
  bootstrap/src/check/queries.c
  bootstrap/src/check/resolution.c
  bootstrap/src/check/result.c
  bootstrap/src/check/service.c
  bootstrap/src/check/service_queries.c
  bootstrap/src/check/type_spelling.c
  bootstrap/src/cli/cli.c
  bootstrap/src/cli/cli_release.c
  bootstrap/src/cli/cli_display.c
  bootstrap/src/cli/cli_heap_limit.c
  bootstrap/src/cli/cli_build_program.c
  bootstrap/src/cli/cli_native_build.c
  bootstrap/src/cli/cli_native_run.c
  bootstrap/src/cli/cli_doctor.c
  bootstrap/src/cli/cli_init.c
  bootstrap/src/cli/cli_capabilities.c
    bootstrap/src/cli/cli_check.c
    bootstrap/src/cli/cli_compile_report.c
    bootstrap/src/cli/cli_mir_pipeline.c
    bootstrap/src/cli/cli_program_arguments.c
    bootstrap/src/cli/cli_run_program.c
    bootstrap/src/cli/cli_test_program.c
    bootstrap/src/cli/cli_run_report.c
    bootstrap/src/cli/cli_resource_report.c
  bootstrap/src/cli/cli_check_targets.c
  bootstrap/src/cli/cli_standard_packages.c
  bootstrap/src/cli/cli_standard_cli_package.c
  bootstrap/src/cli/cli_standard_context_packages.c
  bootstrap/src/cli/cli_standard_json_package.c
  bootstrap/src/cli/cli_format.c
  bootstrap/src/cli/cli_format_targets.c
  bootstrap/src/diagnostic/diagnostic.c
  bootstrap/src/format/document.c
  bootstrap/src/format/formatter.c
  bootstrap/src/format/formatter_collect.c
  bootstrap/src/format/formatter_layout.c
  bootstrap/src/format/formatter_types.c
  bootstrap/src/lex/lexer.c
  bootstrap/src/lex/lexer_scan.c
  bootstrap/src/lex/token.c
  bootstrap/src/manifest/manifest.c
  bootstrap/src/manifest/manifest_format.c
  bootstrap/src/manifest/manifest_parse.c
  bootstrap/src/mir/execution.c
  bootstrap/src/mir/execution_context.c
  bootstrap/src/mir/execution_array.c
  bootstrap/src/mir/execution_collections.c
  bootstrap/src/mir/execution_defer.c
  bootstrap/src/mir/execution_format.c
  bootstrap/src/mir/execution_fs.c
  bootstrap/src/mir/execution_text.c
  bootstrap/src/mir/execution_parse.c
  bootstrap/src/mir/execution_io.c
  bootstrap/src/mir/execution_heap.c
  bootstrap/src/mir/execution_numeric.c
  bootstrap/src/mir/mir.c
  bootstrap/src/mir/mir_collections.c
  bootstrap/src/mir/mir_interfaces.c
  bootstrap/src/mir/integer_range.c
  bootstrap/src/mir/range.c
  bootstrap/src/mir/verify.c
  bootstrap/src/mir/verify_context.c
  bootstrap/src/mir/verify_range.c
  bootstrap/src/mir/verify_interfaces.c
  bootstrap/src/mir/verify_io.c
  bootstrap/src/mir/verify_format.c
  bootstrap/src/mir/verify_fs.c
  bootstrap/src/mir/verify_text.c
  bootstrap/src/mir/verify_parse.c
  bootstrap/src/mir/verify_managed.c
  bootstrap/src/mir/verify_numeric.c
  bootstrap/src/mir/verify_defer.c
  bootstrap/src/mir/verify_support.c
  bootstrap/src/native/build.c
  bootstrap/src/native/runtime_artifact.c
  bootstrap/src/native/toolchain.c
  bootstrap/src/native/c_emitter.c
  bootstrap/src/native/c_emitter_array_fill.c
  bootstrap/src/native/c_emitter_bounds.c
  bootstrap/src/native/c_emitter_relations.c
  bootstrap/src/native/c_emitter_value_ranges.c
  bootstrap/src/native/c_emitter_array_mutation.c
  bootstrap/src/native/c_emitter_descriptors.c
  bootstrap/src/native/c_emitter_defer.c
  bootstrap/src/native/c_emitter_entry.c
  bootstrap/src/native/c_emitter_emit.c
    bootstrap/src/native/c_emitter_format.c
  bootstrap/src/native/c_emitter_io.c
  bootstrap/src/native/c_emitter_fs.c
  bootstrap/src/native/c_emitter_context.c
  bootstrap/src/native/c_emitter_collections.c
  bootstrap/src/native/c_emitter_text.c
  bootstrap/src/native/c_emitter_managed.c
  bootstrap/src/native/c_emitter_roots.c
  bootstrap/src/native/c_emitter_range.c
  bootstrap/src/native/c_emitter_numeric.c
  bootstrap/src/native/c_emitter_validate.c
  bootstrap/src/parse/parser.c
  bootstrap/src/parse/parser_closure.c
  bootstrap/src/parse/parser_declaration.c
  bootstrap/src/parse/parser_expression.c
  bootstrap/src/parse/parser_match.c
  bootstrap/src/session.c
  bootstrap/src/source/source_manager.c
  bootstrap/src/syntax/syntax.c
  bootstrap/src/types/type.c
  runtime/common/directory.c
  runtime/common/env.c
  runtime/common/file.c
  runtime/common/fs.c
  runtime/common/io_stdout.c
  runtime/common/io.c
  runtime/common/test_output.c
  runtime/common/managed.c
  runtime/common/managed_array_access.c
  runtime/common/managed_array_capacity.c
  runtime/common/managed_array_storage.c
  runtime/common/managed_resource.c
  runtime/common/managed_array_mutation.c
  runtime/common/managed_internal.c
  runtime/common/managed_string.c
  runtime/common/managed_record.c
  runtime/common/managed_enum.c
  runtime/common/managed_interface.c
  runtime/common/managed_map.c
  runtime/common/managed_set.c
  runtime/common/program_arguments.c
  runtime/common/process_context.c
  ${MMAY_PLATFORM_SOURCES}
)

target_include_directories(mmay_bootstrap
  PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    bootstrap/include
    runtime/include
)
if(UNIX)
  target_link_libraries(mmay_bootstrap PUBLIC m)
endif()
if(MSVC)
  target_compile_options(mmay_bootstrap PRIVATE /fp:strict)
else()
  target_compile_options(mmay_bootstrap PRIVATE
    -fno-fast-math
    -frounding-math
    -ffp-contract=off
    -fexcess-precision=standard
  )
endif()
mmay_target_warnings(mmay_bootstrap)
mmay_target_sanitizers(mmay_bootstrap)
if(MMAY_BUILD_FUZZERS)
  target_compile_options(mmay_bootstrap PRIVATE -fsanitize=fuzzer-no-link)
endif()

add_executable(mmay bootstrap/main.c)
target_link_libraries(mmay PRIVATE mmay_bootstrap)
add_dependencies(mmay
  mmay_native_runtime_debug
  mmay_native_runtime_release
)
mmay_target_warnings(mmay)
mmay_target_sanitizers(mmay)

install(TARGETS mmay RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS mmay_native_runtime_debug
  ARCHIVE DESTINATION
    "${CMAKE_INSTALL_LIBDIR}/mmay/${MMAY_RUNTIME_VERSION}/native/debug"
)
install(TARGETS mmay_native_runtime_release
  ARCHIVE DESTINATION
    "${CMAKE_INSTALL_LIBDIR}/mmay/${MMAY_RUNTIME_VERSION}/native/release"
)
install(FILES "${MMAY_NATIVE_RUNTIME_DEBUG_MANIFEST}"
  DESTINATION
    "${CMAKE_INSTALL_LIBDIR}/mmay/${MMAY_RUNTIME_VERSION}/native/debug"
)
install(FILES "${MMAY_NATIVE_RUNTIME_RELEASE_MANIFEST}"
  DESTINATION
    "${CMAKE_INSTALL_LIBDIR}/mmay/${MMAY_RUNTIME_VERSION}/native/release"
)
install(DIRECTORY bootstrap/include/mmay runtime/include/mmay
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING PATTERN "*.h"
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/generated/mmay-release-metadata.json"
  cmake/MarimayDistributionLifecycle.cmake
  DESTINATION
    "${CMAKE_INSTALL_DATADIR}/mmay/${MMAY_COMPILER_VERSION}"
)
install(DIRECTORY std
  DESTINATION
    "${CMAKE_INSTALL_DATADIR}/mmay/${MMAY_COMPILER_VERSION}"
  FILES_MATCHING PATTERN "*.my"
)
install(FILES
  README.md
  IMPLEMENTATION-BRIEF.md
  CMakeLists.txt
  CMakePresets.json
  DESTINATION
    "${CMAKE_INSTALL_DATADIR}/doc/mmay/${MMAY_COMPILER_VERSION}"
)
install(DIRECTORY docs examples
  DESTINATION
    "${CMAKE_INSTALL_DATADIR}/doc/mmay/${MMAY_COMPILER_VERSION}"
  USE_SOURCE_PERMISSIONS
  FILES_MATCHING
    PATTERN ".mmay" EXCLUDE
    PATTERN "*.cmake"
    PATTERN "*.in"
    PATTERN "*.py"
    PATTERN "*.md"
    PATTERN "*.tsv"
    PATTERN "*.my"
)
install(DIRECTORY cmake tools
  DESTINATION
    "${CMAKE_INSTALL_DATADIR}/doc/mmay/${MMAY_COMPILER_VERSION}"
  USE_SOURCE_PERMISSIONS
  FILES_MATCHING
    PATTERN "package-distribution"
    PATTERN "check-*"
    PATTERN "generate-*"
    PATTERN "*.cmake"
    PATTERN "*.in"
    PATTERN "*.py"
    PATTERN "*.md"
    PATTERN "*.tsv"
    PATTERN "*.my"
)
install(DIRECTORY bootstrap runtime
  DESTINATION
    "${CMAKE_INSTALL_DATADIR}/doc/mmay/${MMAY_COMPILER_VERSION}"
  FILES_MATCHING
    PATTERN "*.c"
    PATTERN "*.h"
    PATTERN "*.inc"
    PATTERN "*.txt"
    PATTERN "*.tsv"
)
install(DIRECTORY std
  DESTINATION
    "${CMAKE_INSTALL_DATADIR}/doc/mmay/${MMAY_COMPILER_VERSION}"
  FILES_MATCHING PATTERN "*.my"
)
install(SCRIPT
  "${CMAKE_CURRENT_BINARY_DIR}/generated/MarimayInstallMetadata.cmake"
)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  if(WIN32)
    set(MMAY_DISTRIBUTION_ARCHIVE_FORMAT "zip")
  else()
    set(MMAY_DISTRIBUTION_ARCHIVE_FORMAT "tar.gz")
  endif()
  set(MMAY_DISTRIBUTION_STAGE
    "${CMAKE_CURRENT_BINARY_DIR}/distribution/stage"
  )
  string(TOLOWER "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
    MMAY_DISTRIBUTION_HOST
  )
  add_custom_target(mmay_package
    COMMAND "${CMAKE_COMMAND}" -E rm -rf
      "${CMAKE_CURRENT_BINARY_DIR}/distribution"
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_CURRENT_BINARY_DIR}"
      --config "$<CONFIG>" --prefix "${MMAY_DISTRIBUTION_STAGE}"
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/package-distribution"
      --install-root "${MMAY_DISTRIBUTION_STAGE}"
      --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
      --output-directory "${CMAKE_CURRENT_BINARY_DIR}/distribution"
      --version "${MMAY_COMPILER_VERSION}"
      --host "${MMAY_DISTRIBUTION_HOST}"
      --epoch "${MMAY_SOURCE_DATE_EPOCH}"
      --host-format "${MMAY_DISTRIBUTION_ARCHIVE_FORMAT}"
    DEPENDS mmay mmay_native_runtime_debug mmay_native_runtime_release
    COMMENT "Creating deterministic Marimay host and source archives"
    VERBATIM
  )
endif()

if(BUILD_TESTING)
  add_subdirectory(tests/c-unit)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(
      NAME mmay_roadmap
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check-roadmap"
    )
    set_tests_properties(mmay_roadmap PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_custom_target(check-roadmap
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check-roadmap"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "Checking roadmap and checkpoint invariants"
      VERBATIM
    )
    add_test(
      NAME mmay_release_contract
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check-release-contract"
    )
    set_tests_properties(mmay_release_contract PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_custom_target(check-release-contract
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check-release-contract"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "Checking release metadata and CLI0 closure ownership"
      VERBATIM
    )
  endif()
endif()

if(MMAY_BUILD_FUZZERS)
  add_subdirectory(tests/fuzz)
endif()
