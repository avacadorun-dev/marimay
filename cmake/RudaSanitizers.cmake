function(ruda_target_sanitizers target)
  if(NOT RUDA_ENABLE_ADDRESS_SANITIZER AND NOT RUDA_ENABLE_UNDEFINED_SANITIZER
      AND NOT RUDA_ENABLE_THREAD_SANITIZER)
    return()
  endif()

  if(RUDA_ENABLE_THREAD_SANITIZER
      AND (RUDA_ENABLE_ADDRESS_SANITIZER OR RUDA_ENABLE_UNDEFINED_SANITIZER))
    message(FATAL_ERROR "ThreadSanitizer uses a dedicated preset")
  endif()

  if(MSVC)
    if(RUDA_ENABLE_UNDEFINED_SANITIZER OR RUDA_ENABLE_THREAD_SANITIZER)
      message(FATAL_ERROR "Requested sanitizer is not available in the MSVC preset")
    endif()
    target_compile_options(${target} PRIVATE /fsanitize=address)
    target_link_options(${target} PRIVATE /fsanitize=address)
    return()
  endif()

  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "Sanitizer preset is unsupported for ${CMAKE_C_COMPILER_ID}")
  endif()

  set(ruda_sanitizers "")
  if(RUDA_ENABLE_ADDRESS_SANITIZER)
    list(APPEND ruda_sanitizers "address")
  endif()
  if(RUDA_ENABLE_UNDEFINED_SANITIZER)
    list(APPEND ruda_sanitizers "undefined")
  endif()
  if(RUDA_ENABLE_THREAD_SANITIZER)
    list(APPEND ruda_sanitizers "thread")
  endif()
  list(JOIN ruda_sanitizers "," ruda_sanitizer_list)
  target_compile_options(${target} PRIVATE
    "-fsanitize=${ruda_sanitizer_list}"
    -fno-omit-frame-pointer
  )
  target_link_options(${target} PRIVATE "-fsanitize=${ruda_sanitizer_list}")
endfunction()
