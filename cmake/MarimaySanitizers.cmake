function(mmay_target_sanitizers target)
  if(NOT MMAY_ENABLE_ADDRESS_SANITIZER AND NOT MMAY_ENABLE_UNDEFINED_SANITIZER
      AND NOT MMAY_ENABLE_THREAD_SANITIZER)
    return()
  endif()

  if(MMAY_ENABLE_THREAD_SANITIZER
      AND (MMAY_ENABLE_ADDRESS_SANITIZER OR MMAY_ENABLE_UNDEFINED_SANITIZER))
    message(FATAL_ERROR "ThreadSanitizer uses a dedicated preset")
  endif()

  if(MSVC)
    if(MMAY_ENABLE_UNDEFINED_SANITIZER OR MMAY_ENABLE_THREAD_SANITIZER)
      message(FATAL_ERROR "Requested sanitizer is not available in the MSVC preset")
    endif()
    mmay_disable_msvc_runtime_checks(${target})
    target_compile_options(${target} PRIVATE /fsanitize=address)
    # Instrumented object metadata selects the ASan libraries in MSVC. The
    # compiler's /fsanitize switch is not a link.exe option; incremental linking
    # is incompatible with ASan.
    target_link_options(${target} PRIVATE /INCREMENTAL:NO)
    return()
  endif()

  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "Sanitizer preset is unsupported for ${CMAKE_C_COMPILER_ID}")
  endif()

  set(mmay_sanitizers "")
  if(MMAY_ENABLE_ADDRESS_SANITIZER)
    list(APPEND mmay_sanitizers "address")
  endif()
  if(MMAY_ENABLE_UNDEFINED_SANITIZER)
    list(APPEND mmay_sanitizers "undefined")
  endif()
  if(MMAY_ENABLE_THREAD_SANITIZER)
    list(APPEND mmay_sanitizers "thread")
  endif()
  list(JOIN mmay_sanitizers "," mmay_sanitizer_list)
  # Exhaustive allocation-failure sweeps are prohibitively slow at Debug's
  # implicit -O0. Keep debug symbols and assertions, but use the optimization
  # level recommended for practical sanitizer instrumentation. Native runtime
  # profile targets append their exact -O0/-O2 contract after this helper.
  target_compile_options(${target} PRIVATE
    "-fsanitize=${mmay_sanitizer_list}"
    -fno-omit-frame-pointer
    -O1
  )
  target_link_options(${target} PRIVATE "-fsanitize=${mmay_sanitizer_list}")
endfunction()
