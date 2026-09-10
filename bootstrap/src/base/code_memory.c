#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif
#include "mmay/base/code_memory.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#if defined(__APPLE__) && defined(__aarch64__) && !defined(__arm64e__)
#include <pthread.h>
#include <libkern/OSCacheControl.h>
typedef struct MarimayCodePublication {
    MarimayCodeMemory *memory;
    const void *source;
    size_t bytes;
} MarimayCodePublication;
static int mmay_code_memory_write(void *opaque) {
    const MarimayCodePublication *p = opaque;
    if (p == NULL || p->memory == NULL || p->source == NULL || p->bytes == 0
        || p->memory->address == NULL || p->memory->published
        || p->bytes > p->memory->bytes) return -1;
    memcpy(p->memory->address, p->source, p->bytes);
    return 0;
}
PTHREAD_JIT_WRITE_ALLOW_CALLBACKS_NP(mmay_code_memory_write);
#endif

static bool mmay_code_memory_fault(MarimayCodeMemoryFault *fault) {
    if (fault == NULL) return false;
    size_t index = fault->attempts;
    if (fault->attempts != SIZE_MAX) fault->attempts += 1;
    return fault->failure_index != SIZE_MAX && index == fault->failure_index;
}
bool mmay_code_memory_supported(void) {
#if defined(__arm64e__)
    return false;
#elif defined(__APPLE__) && defined(__aarch64__)
    if (__builtin_available(macOS 11.4, *)) return pthread_jit_write_protect_supported_np() != 0;
    return false;
#elif (defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64) || defined(_M_ARM64)) && (defined(_WIN32) || defined(__linux__) || defined(__APPLE__))
    return true;
#else
    return false;
#endif
}
static size_t mmay_code_memory_page_size(void) {
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); return info.dwPageSize;
#elif defined(__APPLE__) || defined(__linux__)
    long size = sysconf(_SC_PAGESIZE); return size > 0 ? (size_t)size : 0;
#else
    return 0;
#endif
}
MarimayCodeMemoryStatus mmay_code_memory_reserve(MarimayCodeMemory *memory,
    size_t bytes, size_t limit, MarimayCodeMemoryFault *fault) {
    if (memory == NULL || memory->address != NULL || bytes == 0) return MMAY_CODE_MEMORY_INVALID;
    if (!mmay_code_memory_supported()) return MMAY_CODE_MEMORY_UNAVAILABLE;
    size_t page = mmay_code_memory_page_size();
    if (page == 0) return MMAY_CODE_MEMORY_UNAVAILABLE;
    size_t tail = bytes % page, extra = tail ? page - tail : 0;
    if (bytes > SIZE_MAX - extra || bytes + extra > limit) return MMAY_CODE_MEMORY_LIMIT;
    bytes += extra;
    if (fault != NULL && (fault->live_mappings == SIZE_MAX || fault->live_bytes > SIZE_MAX - bytes)) return MMAY_CODE_MEMORY_LIMIT;
    if (mmay_code_memory_fault(fault)) return MMAY_CODE_MEMORY_ALLOCATION_FAILED;
    void *address = NULL;
#if defined(_WIN32)
    address = VirtualAlloc(NULL, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (address == NULL) return GetLastError() == ERROR_NOT_ENOUGH_MEMORY || GetLastError() == ERROR_OUTOFMEMORY
        ? MMAY_CODE_MEMORY_ALLOCATION_FAILED : MMAY_CODE_MEMORY_UNAVAILABLE;
#elif defined(__APPLE__) || defined(__linux__)
    int protection = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANON;
#if defined(__APPLE__) && defined(__aarch64__) && !defined(__arm64e__)
    /* MAP_JIT is governed by per-thread W^X. Only the allowlisted callback writes. */
    protection |= PROT_EXEC; flags |= MAP_JIT;
#endif
    address = mmap(NULL, bytes, protection, flags, -1, 0);
    if (address == MAP_FAILED) return errno == ENOMEM ? MMAY_CODE_MEMORY_ALLOCATION_FAILED : MMAY_CODE_MEMORY_UNAVAILABLE;
#else
    return MMAY_CODE_MEMORY_UNAVAILABLE;
#endif
    memory->address = address; memory->bytes = bytes; memory->published = false; memory->fault = fault;
    if (fault != NULL) { fault->live_mappings += 1; fault->live_bytes += bytes; }
    return MMAY_CODE_MEMORY_OK;
}
MarimayCodeMemoryStatus mmay_code_memory_publish(MarimayCodeMemory *memory,
    const void *source, size_t bytes) {
    if (memory == NULL || memory->address == NULL || memory->published
        || source == NULL || bytes == 0 || bytes > memory->bytes) return MMAY_CODE_MEMORY_INVALID;
    if (mmay_code_memory_fault(memory->fault)) return MMAY_CODE_MEMORY_UNAVAILABLE;
#if defined(__APPLE__) && defined(__aarch64__) && !defined(__arm64e__)
    if (__builtin_available(macOS 11.4, *)) {
        MarimayCodePublication publication = {memory, source, bytes};
        if (pthread_jit_write_with_callback_np(mmay_code_memory_write, &publication) != 0) return MMAY_CODE_MEMORY_UNAVAILABLE;
        sys_icache_invalidate(memory->address, bytes);
    } else return MMAY_CODE_MEMORY_UNAVAILABLE;
#elif defined(_WIN32)
    memcpy(memory->address, source, bytes);
    DWORD old = 0;
    if (!VirtualProtect(memory->address, memory->bytes, PAGE_EXECUTE_READ, &old)
        || !FlushInstructionCache(GetCurrentProcess(), memory->address, bytes)) return MMAY_CODE_MEMORY_UNAVAILABLE;
#elif defined(__linux__) || defined(__APPLE__)
    memcpy(memory->address, source, bytes);
    if (mprotect(memory->address, memory->bytes, PROT_READ | PROT_EXEC) != 0) return MMAY_CODE_MEMORY_UNAVAILABLE;
    __builtin___clear_cache(memory->address, (char *)memory->address + bytes);
#else
    return MMAY_CODE_MEMORY_UNAVAILABLE;
#endif
    memory->published = true;
    return MMAY_CODE_MEMORY_OK;
}
void mmay_code_memory_dispose(MarimayCodeMemory *memory) {
    if (memory == NULL) return;
    if (memory->address != NULL) {
#if defined(_WIN32)
        if (!VirtualFree(memory->address, 0, MEM_RELEASE)) abort();
#elif defined(__APPLE__) || defined(__linux__)
        if (munmap(memory->address, memory->bytes) != 0) abort();
#endif
        /* A failed release violates ownership; never report it as a zero ledger. */
        if (memory->fault != NULL) { memory->fault->live_mappings -= 1; memory->fault->live_bytes -= memory->bytes; }
    }
    memset(memory, 0, sizeof(*memory));
}
