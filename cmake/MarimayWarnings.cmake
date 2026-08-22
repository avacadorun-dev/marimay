function(mmay_target_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(MMAY_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  elseif(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wshadow
    )
    if(MMAY_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  else()
    message(WARNING "No maintained warning profile for ${CMAKE_C_COMPILER_ID}")
  endif()
endfunction()
