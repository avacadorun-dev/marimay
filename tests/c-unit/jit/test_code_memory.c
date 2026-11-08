#include "vm_jit_memory.h"
#include "sljitLir.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdalign.h>
#if defined(__APPLE__) || defined(__linux__)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#define EXPECT(condition) do { if (!(condition)) { fprintf(stderr,"memory test failed: %s at %d\n",#condition,__LINE__); exit(1); } } while (0)

static intptr_t helper(intptr_t value) { return value * 3 + 7; }
static MarimayCodeMemoryStatus compile(MarimayJitCodeBuild *build) {
    struct sljit_compiler *compiler = sljit_create_compiler(&build->allocator);
    if (compiler == NULL) return MMAY_CODE_MEMORY_ALLOCATION_FAILED;
    sljit_emit_enter(compiler, 0, SLJIT_ARGS1(W, W), 4, 1, 0);
    struct sljit_jump *minimum = sljit_emit_cmp(compiler, SLJIT_EQUAL, SLJIT_S0, 0, SLJIT_IMM, INTPTR_MIN);
    struct sljit_jump *negative = sljit_emit_cmp(compiler, SLJIT_SIG_LESS, SLJIT_S0, 0, SLJIT_IMM, 0);
    sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    for (size_t i = 0; i < 1024; ++i)
        sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
    intptr_t (*called)(intptr_t) = helper;
    sljit_sw address = 0;
    _Static_assert(sizeof(address) == sizeof(called), "function pointer");
    memcpy(&address, &called, sizeof(address));
    sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS1(W, W), SLJIT_IMM, address);
    sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);
    sljit_set_label(negative, sljit_emit_label(compiler));
    sljit_emit_return(compiler, SLJIT_MOV, SLJIT_IMM, -77);
    sljit_set_label(minimum, sljit_emit_label(compiler));
    sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, INTPTR_MIN);
    sljit_emit_return(compiler, SLJIT_MOV, SLJIT_R0, 0);
    void *code = sljit_generate_code(compiler, 0, build);
    MarimayCodeMemoryStatus status = code
        ? mmay_jit_code_publish(build, code, sljit_get_generated_code_size(compiler))
        : build->status != MMAY_CODE_MEMORY_OK ? build->status : MMAY_CODE_MEMORY_ALLOCATION_FAILED;
    sljit_free_compiler(compiler);
    return status;
}
#if defined(__APPLE__) || defined(__linux__)
static void denied_write(int signal_number) { (void)signal_number; _exit(0); }
static void check_immutable(void *address) {
    pid_t child = fork();
    EXPECT(child >= 0);
    if (child == 0) {
        signal(SIGSEGV, denied_write); signal(SIGBUS, denied_write);
        volatile unsigned char *byte = address;
        *byte ^= 1;
        _exit(1);
    }
    int result = 0;
    EXPECT(waitpid(child, &result, 0) == child);
    EXPECT(WIFEXITED(result) && WEXITSTATUS(result) == 0);
}
#endif

typedef struct Observation { size_t allocations, platform_attempts; MarimayCodeMemoryStatus status; } Observation;
static Observation observe(size_t allocation_failure, size_t platform_failure, size_t capacity, bool protection) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), allocation_failure);
    MarimayCodeMemoryFault faults = {platform_failure, 0, 0, 0};
    MarimayJitCodeBuild build = {.allocator=mmay_fail_allocator(&failing), .capacity=capacity, .fault=&faults};
    MarimayCodeMemoryStatus status = compile(&build);
    if (status == MMAY_CODE_MEMORY_OK) {
        EXPECT(build.memory.published && build.memory.address != NULL && build.staging == NULL);
        EXPECT(build.memory.bytes <= capacity && faults.live_mappings == 1 && faults.live_bytes == build.memory.bytes);
        intptr_t (*entry)(intptr_t) = NULL;
        _Static_assert(sizeof(entry) == sizeof(build.memory.address), "entry pointer");
        memcpy(&entry, &build.memory.address, sizeof(entry));
        EXPECT(entry(0) == 3079 && entry(4) == 3091 && entry(-5) == -77 && entry(INTPTR_MIN) == 0);
        EXPECT(mmay_code_memory_publish(&build.memory, "x", 1) == MMAY_CODE_MEMORY_INVALID);
        EXPECT(mmay_code_memory_reserve(&build.memory, 1, capacity, &faults) == MMAY_CODE_MEMORY_INVALID);
        EXPECT(entry(0) == 3079);
#if defined(__APPLE__) || defined(__linux__)
        if (protection) check_immutable(build.memory.address);
#else
        (void)protection;
#endif
    }
    mmay_jit_code_dispose(&build);
    mmay_jit_code_dispose(&build);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    EXPECT(faults.live_mappings == 0 && faults.live_bytes == 0);
    return (Observation){failing.attempts, faults.attempts, status};
}
static void argument_contract(void) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&failing);
    EXPECT(mmay_sljit_allocate(0, &allocator) == NULL);
    EXPECT(mmay_sljit_allocate(SIZE_MAX, &allocator) == NULL);
    EXPECT(mmay_sljit_allocate(1, NULL) == NULL);
    EXPECT(failing.attempts == 0);
    void *memory = mmay_sljit_allocate(1, &allocator);
    EXPECT(memory != NULL && (uintptr_t)memory % alignof(MarimayMaxAlign) == 0);
    mmay_sljit_deallocate(memory, &allocator);
    mmay_sljit_deallocate(NULL, &allocator);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    MarimayCodeMemory map = {0};
    EXPECT(mmay_code_memory_reserve(&map, 0, SIZE_MAX, NULL) == MMAY_CODE_MEMORY_INVALID);
    EXPECT(mmay_code_memory_publish(&map, "x", 1) == MMAY_CODE_MEMORY_INVALID);
    EXPECT(mmay_code_memory_reserve(NULL, 1, SIZE_MAX, NULL) == MMAY_CODE_MEMORY_INVALID);
    mmay_code_memory_dispose(&map); mmay_code_memory_dispose(NULL);
    if (mmay_code_memory_supported()) {
        EXPECT(mmay_code_memory_reserve(&map, SIZE_MAX, SIZE_MAX, NULL) == MMAY_CODE_MEMORY_LIMIT);
        MarimayCodeMemoryFault faults = {SIZE_MAX, 0, SIZE_MAX, 0};
        EXPECT(mmay_code_memory_reserve(&map, 1, SIZE_MAX, &faults) == MMAY_CODE_MEMORY_LIMIT);
        faults = (MarimayCodeMemoryFault){SIZE_MAX, 0, 0, SIZE_MAX};
        EXPECT(mmay_code_memory_reserve(&map, 1, SIZE_MAX, &faults) == MMAY_CODE_MEMORY_LIMIT);
        EXPECT(faults.attempts == 0 && map.address == NULL);
        faults = (MarimayCodeMemoryFault){SIZE_MAX, 0, 0, 0};
        MarimayJitCodeBuild build = {.allocator=allocator, .capacity=8U*1024U*1024U, .fault=&faults};
        EXPECT(mmay_sljit_code_allocate(1, &build) != NULL);
        EXPECT(build.staging_bytes == 1 && build.memory.bytes > 1);
        EXPECT(mmay_jit_code_publish(&build, build.memory.address, 2) == MMAY_CODE_MEMORY_INVALID);
        EXPECT(mmay_jit_code_publish(&build, build.memory.address, 0) == MMAY_CODE_MEMORY_INVALID);
        EXPECT(mmay_jit_code_publish(&build, build.staging, 1) == MMAY_CODE_MEMORY_INVALID);
        EXPECT(!build.memory.published && build.staging != NULL && faults.attempts == 1);
        EXPECT(mmay_sljit_code_allocate(1, &build) == NULL);
        mmay_jit_code_dispose(&build);
        EXPECT(build.staging_bytes == 0 && faults.live_mappings == 0 && faults.live_bytes == 0);
        EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
        faults.attempts = SIZE_MAX;
        EXPECT(mmay_code_memory_reserve(&map, 1, SIZE_MAX, &faults) == MMAY_CODE_MEMORY_OK);
        EXPECT(faults.attempts == SIZE_MAX);
        mmay_code_memory_dispose(&map);
        EXPECT(faults.live_mappings == 0 && faults.live_bytes == 0);
    }
}
int main(void) {
    argument_contract();
    if (!mmay_code_memory_supported()) { puts("code memory unavailable on this host"); return 77; }
    Observation control = observe(SIZE_MAX, SIZE_MAX, 8U*1024U*1024U, true);
    EXPECT(control.status == MMAY_CODE_MEMORY_OK && control.allocations > 4 && control.platform_attempts == 2);
    for (size_t i = 0; i < control.allocations; ++i) {
        Observation failure = observe(i, SIZE_MAX, 8U*1024U*1024U, false);
        EXPECT(failure.status == MMAY_CODE_MEMORY_ALLOCATION_FAILED);
    }
    for (size_t i = 0; i < control.platform_attempts; ++i) {
        Observation failure = observe(SIZE_MAX, i, 8U*1024U*1024U, false);
        EXPECT(failure.status == (i == 0 ? MMAY_CODE_MEMORY_ALLOCATION_FAILED : MMAY_CODE_MEMORY_UNAVAILABLE));
    }
    EXPECT(observe(control.allocations, SIZE_MAX, 8U*1024U*1024U, false).status == MMAY_CODE_MEMORY_OK);
    EXPECT(observe(SIZE_MAX, SIZE_MAX, 1, false).status == MMAY_CODE_MEMORY_LIMIT);
    printf("%s: %zu allocation failure indices, %zu platform failure indices, relocated calls/branches, immutable publication, zero allocation/map counters: passed\n",sljit_get_platform_name(),control.allocations,control.platform_attempts);
    return 0;
}
