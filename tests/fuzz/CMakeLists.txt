set(MMAY_FUZZ_TARGETS
  bytecode
  frontend
  formatter
)

foreach(mmay_fuzz_target IN LISTS MMAY_FUZZ_TARGETS)
  add_executable(mmay_fuzz_${mmay_fuzz_target}
    ${mmay_fuzz_target}_fuzz.c
    semantic_oracle.c
  )
  target_link_libraries(mmay_fuzz_${mmay_fuzz_target} PRIVATE mmay_bootstrap)
  target_include_directories(mmay_fuzz_${mmay_fuzz_target} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_compile_options(mmay_fuzz_${mmay_fuzz_target} PRIVATE -fsanitize=fuzzer)
  target_link_options(mmay_fuzz_${mmay_fuzz_target} PRIVATE -fsanitize=fuzzer)
  mmay_target_warnings(mmay_fuzz_${mmay_fuzz_target})
  mmay_target_sanitizers(mmay_fuzz_${mmay_fuzz_target})
endforeach()
