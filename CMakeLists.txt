cmake_minimum_required(VERSION 3.25)

project(Ruda VERSION 0.0.0 LANGUAGES C)

include(CTest)
include(cmake/RudaWarnings.cmake)
include(cmake/RudaSanitizers.cmake)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(RUDA_WARNINGS_AS_ERRORS "Treat warnings in Ruda project code as errors" ON)
option(RUDA_ENABLE_ADDRESS_SANITIZER "Enable AddressSanitizer" OFF)
option(RUDA_ENABLE_UNDEFINED_SANITIZER "Enable UndefinedBehaviorSanitizer" OFF)
option(RUDA_ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer" OFF)
option(RUDA_BUILD_FUZZERS "Build Clang libFuzzer frontend targets" OFF)

if(RUDA_BUILD_FUZZERS)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Ruda libFuzzer targets require Clang")
  endif()
  include(CheckCSourceCompiles)
  set(RUDA_SAVED_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fsanitize=fuzzer")
  check_c_source_compiles(
    "#include <stddef.h>
     int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
       return data == NULL && size != 0;
     }"
    RUDA_COMPILER_HAS_LIBFUZZER
  )
  set(CMAKE_REQUIRED_FLAGS "${RUDA_SAVED_REQUIRED_FLAGS}")
  if(NOT RUDA_COMPILER_HAS_LIBFUZZER)
    message(FATAL_ERROR
      "RUDA_BUILD_FUZZERS requires a Clang distribution with the libFuzzer runtime"
    )
  endif()
endif()

if(WIN32)
  set(RUDA_PLATFORM_SOURCES
    runtime/platform/windows/directory.c
    runtime/platform/windows/file.c
    runtime/platform/windows/process.c
  )
elseif(UNIX)
  set(RUDA_PLATFORM_SOURCES
    runtime/platform/posix/directory.c
    runtime/platform/posix/file.c
    runtime/platform/posix/process.c
  )
else()
  message(FATAL_ERROR "No Stage 0 platform adapter for this host")
endif()

set(RUDA_COMPILER_VERSION "0.0.0-dev")
set(RUDA_RUNTIME_VERSION "0.0.0-dev")
set(RUDA_LANGUAGE_VERSION "0.1-design")
set(RUDA_BYTECODE_VERSION "unavailable")
set(RUDA_STANDARD_LIBRARY_VERSION "unavailable")
set(RUDA_BOOTSTRAP_STAGE 0)

configure_file(
  cmake/ruda_build_config.h.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/ruda/build_config.h"
  @ONLY
)

add_library(ruda_bootstrap STATIC
  bootstrap/src/ast/ast.c
  bootstrap/src/base/allocator.c
  bootstrap/src/base/arena.c
  bootstrap/src/base/checked_i64.c
  bootstrap/src/base/f64.c
  bootstrap/src/base/f64_runtime.c
  bootstrap/src/base/hash.c
  bootstrap/src/base/map.c
  bootstrap/src/base/owned_bytes.c
  bootstrap/src/base/path.c
  bootstrap/src/base/provenance.c
  bootstrap/src/base/stream.c
  bootstrap/src/base/string_literal.c
  bootstrap/src/base/unicode.c
  bootstrap/src/base/utf8.c
  bootstrap/src/base/vector.c
  bootstrap/src/base/writer.c
  bootstrap/src/bytecode/bytecode.c
  bootstrap/src/bytecode/bytecode_arrays.c
  bootstrap/src/bytecode/bytecode_nominal.c
  bootstrap/src/bytecode/bytecode_codec.c
  bootstrap/src/bytecode/bytecode_codec_nominal.c
  bootstrap/src/bytecode/bytecode_codec_nominal_values.c
  bootstrap/src/bytecode/bytecode_verify.c
  bootstrap/src/bytecode/bytecode_verify_instructions.c
  bootstrap/src/bytecode/bytecode_verify_nominal.c
  bootstrap/src/bytecode/bytecode_verify_provenance.c
  bootstrap/src/bytecode/program.c
  bootstrap/src/bytecode/program_limits.c
  bootstrap/src/bytecode/program_verify_nominal.c
  bootstrap/src/bytecode/mir_lower.c
  bootstrap/src/bytecode/mir_lower_nominal.c
  bootstrap/src/bytecode/vm.c
  bootstrap/src/bytecode/vm_interface.c
  bootstrap/src/bytecode/vm_nominal.c
  bootstrap/src/bytecode/vm_prepare.c
  bootstrap/src/bytecode/vm_value.c
  bootstrap/src/check/array.c
  bootstrap/src/check/checker.c
  bootstrap/src/check/checker_cfg.c
  bootstrap/src/check/checker_core.c
  bootstrap/src/check/checker_declaration_constants.c
  bootstrap/src/check/checker_declaration_enums.c
  bootstrap/src/check/checker_declaration_functions.c
  bootstrap/src/check/checker_declaration_interfaces.c
  bootstrap/src/check/checker_declaration_lifecycle.c
  bootstrap/src/check/checker_declaration_nominal.c
  bootstrap/src/check/checker_declaration_publication_nominal.c
  bootstrap/src/check/checker_declaration_records.c
  bootstrap/src/check/checker_expr_calls.c
  bootstrap/src/check/checker_expr_prelude.c
  bootstrap/src/check/checker_expr_try.c
  bootstrap/src/check/checker_expr_map_error.c
  bootstrap/src/check/checker_expr_interfaces.c
  bootstrap/src/check/checker_generic_types.c
  bootstrap/src/check/checker_generic_specialization.c
  bootstrap/src/check/checker_prelude.c
  bootstrap/src/check/checker_expr_nominal.c
  bootstrap/src/check/checker_expr_records.c
  bootstrap/src/check/checker_expr_scalar.c
  bootstrap/src/check/checker_expression.c
  bootstrap/src/check/checker_interpret.c
  bootstrap/src/check/checker_imported_nominal.c
  bootstrap/src/check/checker_lookup.c
  bootstrap/src/check/checker_lookup_imports.c
  bootstrap/src/check/checker_match_analysis.c
  bootstrap/src/check/checker_match_binding.c
  bootstrap/src/check/checker_match_dispatch.c
  bootstrap/src/check/checker_match_lower.c
  bootstrap/src/check/checker_match_pattern.c
  bootstrap/src/check/checker_match_witness.c
  bootstrap/src/check/checker_publication.c
  bootstrap/src/check/checker_statement.c
  bootstrap/src/check/facts.c
  bootstrap/src/check/package_graph.c
  bootstrap/src/check/queries.c
  bootstrap/src/check/resolution.c
  bootstrap/src/check/result.c
  bootstrap/src/check/service.c
  bootstrap/src/check/service_queries.c
  bootstrap/src/cli/cli.c
  bootstrap/src/cli/cli_format.c
  bootstrap/src/cli/cli_format_targets.c
  bootstrap/src/diagnostic/diagnostic.c
  bootstrap/src/format/document.c
  bootstrap/src/format/formatter.c
  bootstrap/src/format/formatter_collect.c
  bootstrap/src/lex/lexer.c
  bootstrap/src/lex/lexer_scan.c
  bootstrap/src/lex/token.c
  bootstrap/src/manifest/manifest.c
  bootstrap/src/manifest/manifest_format.c
  bootstrap/src/manifest/manifest_parse.c
  bootstrap/src/mir/execution.c
  bootstrap/src/mir/execution_heap.c
  bootstrap/src/mir/mir.c
  bootstrap/src/mir/mir_interfaces.c
  bootstrap/src/mir/verify.c
  bootstrap/src/mir/verify_interfaces.c
  bootstrap/src/mir/verify_managed.c
  bootstrap/src/mir/verify_support.c
  bootstrap/src/native/c_emitter.c
  bootstrap/src/native/c_emitter_descriptors.c
  bootstrap/src/native/c_emitter_emit.c
  bootstrap/src/native/c_emitter_managed.c
  bootstrap/src/native/c_emitter_validate.c
  bootstrap/src/parse/parser.c
  bootstrap/src/parse/parser_declaration.c
  bootstrap/src/parse/parser_expression.c
  bootstrap/src/session.c
  bootstrap/src/source/source_manager.c
  bootstrap/src/syntax/syntax.c
  bootstrap/src/types/type.c
  runtime/common/directory.c
  runtime/common/file.c
  runtime/common/managed.c
  runtime/common/managed_internal.c
  runtime/common/managed_string.c
  runtime/common/managed_record.c
  runtime/common/managed_enum.c
  runtime/common/managed_interface.c
  ${RUDA_PLATFORM_SOURCES}
)

target_include_directories(ruda_bootstrap
  PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    bootstrap/include
    runtime/include
)
if(UNIX)
  target_link_libraries(ruda_bootstrap PUBLIC m)
endif()
if(MSVC)
  target_compile_options(ruda_bootstrap PRIVATE /fp:strict)
else()
  target_compile_options(ruda_bootstrap PRIVATE
    -fno-fast-math
    -frounding-math
    -ffp-contract=off
    -fexcess-precision=standard
  )
endif()
ruda_target_warnings(ruda_bootstrap)
ruda_target_sanitizers(ruda_bootstrap)
if(RUDA_BUILD_FUZZERS)
  target_compile_options(ruda_bootstrap PRIVATE -fsanitize=fuzzer-no-link)
endif()

add_executable(ruda bootstrap/main.c)
target_link_libraries(ruda PRIVATE ruda_bootstrap)
ruda_target_warnings(ruda)
ruda_target_sanitizers(ruda)

if(BUILD_TESTING)
  add_subdirectory(tests/c-unit)
endif()

if(RUDA_BUILD_FUZZERS)
  add_subdirectory(tests/fuzz)
endif()
