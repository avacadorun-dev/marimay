set(RUDA_FUZZ_TARGETS
  bytecode
  frontend
  formatter
)

foreach(ruda_fuzz_target IN LISTS RUDA_FUZZ_TARGETS)
  add_executable(ruda_fuzz_${ruda_fuzz_target}
    ${ruda_fuzz_target}_fuzz.c
    semantic_oracle.c
  )
  target_link_libraries(ruda_fuzz_${ruda_fuzz_target} PRIVATE ruda_bootstrap)
  target_include_directories(ruda_fuzz_${ruda_fuzz_target} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_compile_options(ruda_fuzz_${ruda_fuzz_target} PRIVATE -fsanitize=fuzzer)
  target_link_options(ruda_fuzz_${ruda_fuzz_target} PRIVATE -fsanitize=fuzzer)
  ruda_target_warnings(ruda_fuzz_${ruda_fuzz_target})
  ruda_target_sanitizers(ruda_fuzz_${ruda_fuzz_target})
endforeach()
