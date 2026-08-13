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
  bootstrap/src/bytecode/program.c
  bootstrap/src/bytecode/mir_lower.c
  bootstrap/src/bytecode/vm.c
  bootstrap/src/check/checker.c
  bootstrap/src/cli/cli.c
  bootstrap/src/diagnostic/diagnostic.c
  bootstrap/src/format/document.c
  bootstrap/src/format/formatter.c
  bootstrap/src/lex/lexer.c
  bootstrap/src/lex/token.c
  bootstrap/src/manifest/manifest.c
  bootstrap/src/mir/mir.c
  bootstrap/src/native/c_emitter.c
  bootstrap/src/parse/parser.c
  bootstrap/src/session.c
  bootstrap/src/source/source_manager.c
  bootstrap/src/syntax/syntax.c
  bootstrap/src/types/type.c
  runtime/common/directory.c
  runtime/common/file.c
  runtime/common/managed.c
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
