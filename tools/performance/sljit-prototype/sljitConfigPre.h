#ifndef MMAY_SLJIT_CONFIG_PRE_H
#define MMAY_SLJIT_CONFIG_PRE_H
#include "jit_memory.h"
#define SLJIT_EXECUTABLE_ALLOCATOR 0
#define SLJIT_PROT_EXECUTABLE_ALLOCATOR 1
#define SLJIT_MALLOC(size, allocator) mmay_sljit_allocate((size), (allocator))
#define SLJIT_FREE(pointer, allocator) mmay_sljit_deallocate((pointer), (allocator))
#define SLJIT_MALLOC_EXEC(size, context) mmay_sljit_code_allocate((size), (context))
#define SLJIT_FREE_EXEC(pointer, context) mmay_sljit_code_deallocate((pointer), (context))
#define SLJIT_EXEC_OFFSET(pointer) mmay_sljit_code_offset((pointer))
/* SLJIT writes only the non-executable staging allocation. The publication
 * owner copies once to the reserved address and performs W^X/cache changes. */
#define SLJIT_CACHE_FLUSH(from, to) do { (void)(from); (void)(to); } while (0)
#define SLJIT_UPDATE_WX_FLAGS(from, to, executable) do { (void)(from); (void)(to); (void)(executable); } while (0)
#endif
