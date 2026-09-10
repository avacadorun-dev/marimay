#include "vm_jit_internal.h"
#include <stdio.h>
#include <stdlib.h>

#define EXPECT(condition) do { if (!(condition)) { fprintf(stderr, "JIT ownership failed: %s at %d\n", #condition, __LINE__); exit(1); } } while (0)

/* Two borrowed scalar images isolate this owner's lifecycle from verification
 * and managed allocation. Existing bytecode suites exercise those boundaries. */
typedef struct Fixture {
    MarimayBytecodeType types[2];
    MarimayBytecodeProgramFunction functions[2];
    MarimayBytecodeImageOperation operations[5];
    uint32_t starts[2];
    MarimayBytecodeImageFunction images[2];
    MarimayBytecodeProgram program;
    MarimayBytecodeExecutionImage image;
} Fixture;

static void fixture_init(Fixture *fixture) {
    *fixture = (Fixture){0};
    fixture->types[0] = fixture->types[1] = MMAY_BYTECODE_TYPE_I64;
    fixture->operations[0] = (MarimayBytecodeImageOperation){.opcode=MMAY_OPCODE_CONST_I64};
    fixture->operations[1] = (MarimayBytecodeImageOperation){.opcode=MMAY_OPCODE_CONST_I64,
        .first=1, .immediate=1, .source_instruction=1};
    fixture->operations[2] = (MarimayBytecodeImageOperation){.opcode=MMAY_OPCODE_JUMP,
        .first=3, .source_instruction=2};
    fixture->operations[3] = (MarimayBytecodeImageOperation){.opcode=MMAY_OPCODE_I64_ADD_CHECKED,
        .first=0, .second=0, .third=1, .source_block=1};
    fixture->operations[4] = (MarimayBytecodeImageOperation){.opcode=MMAY_OPCODE_JUMP,
        .first=3, .source_block=1, .source_instruction=1};
    fixture->starts[1] = 3;
    for (size_t i = 0; i < 2; ++i) {
        fixture->functions[i].body.register_types = (MarimayVector){
            .data=fixture->types, .length=2, .capacity=2, .element_size=sizeof(*fixture->types)};
        fixture->images[i] = (MarimayBytecodeImageFunction){.operations=fixture->operations,
            .block_starts=fixture->starts, .operation_count=5, .block_count=2};
    }
    fixture->program.functions = (MarimayVector){.data=fixture->functions,
        .length=2, .capacity=2, .element_size=sizeof(*fixture->functions)};
    fixture->image.functions = fixture->images;
    fixture->image.function_count = 2;
}

static size_t observe(size_t allocation_failure, size_t platform_failure,
    bool succeeds, MarimayBytecodeRunStatus expected, bool meter) {
    Fixture fixture;
    fixture_init(&fixture);
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), allocation_failure);
    MarimayCodeMemoryFault fault = {platform_failure, 0, 0, 0};
    MarimayBytecodeJit jit = {0};
    EXPECT(mmay_bytecode_jit_init(&jit, &fixture.program, &fixture.image,
        mmay_fail_allocator(&failing), &fault, meter) == expected);
    EXPECT(jit.ready == succeeds);
    if (succeeds) {
        EXPECT(jit.function_count == 2 && fault.attempts == 4 && fault.live_mappings == 2);
        EXPECT(fault.live_bytes == jit.functions[0].memory.bytes + jit.functions[1].memory.bytes);
        EXPECT(fault.live_bytes <= MMAY_JIT_MAX_CODE_BYTES);
        for (size_t id = 0; id < 2; ++id) {
            MarimayBytecodeValue registers[2] = {{.type=MMAY_BYTECODE_TYPE_I64}, {.type=MMAY_BYTECODE_TYPE_I64}};
            size_t block = 0, instruction = 0, steps = 0, shared = 10;
            MarimayBytecodeRunOptions options = {.max_steps=10, .remaining_shared_steps=&shared};
            mmay_bytecode_jit_run(&jit, id, &fixture.images[id], &fixture.functions[id].body,
                registers, NULL, &block, &instruction, &steps, options);
            if (meter) {
                EXPECT(registers[0].as.i64 == 3 && registers[1].as.i64 == 1);
                EXPECT(block == 1 && instruction == 0 && steps == 9 && shared == 1);
            } else {
                EXPECT(registers[0].as.i64 == 0 && registers[1].as.i64 == 0);
                EXPECT(block == 0 && instruction == 0 && steps == 0 && shared == 10);
            }
        }
    } else {
        EXPECT(jit.metadata == NULL && jit.bindings == NULL && jit.function_count == 0);
        EXPECT(fault.live_mappings == 0 && fault.live_bytes == 0);
    }
    mmay_bytecode_jit_dispose(&jit);
    mmay_bytecode_jit_dispose(&jit);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    EXPECT(fault.live_mappings == 0 && fault.live_bytes == 0);
    return failing.attempts;
}

int main(void) {
    if (!mmay_code_memory_supported()) return 77;
    for (size_t mode = 0; mode < 2; ++mode) {
        bool meter = mode != 0;
        size_t allocations = observe(SIZE_MAX, SIZE_MAX, true, MMAY_BYTECODE_RUN_OK, meter);
        for (size_t i = 0; i < allocations; ++i)
            observe(i, SIZE_MAX, false, MMAY_BYTECODE_RUN_ALLOCATION_FAILED, meter);
        observe(allocations, SIZE_MAX, true, MMAY_BYTECODE_RUN_OK, meter);
        for (size_t i = 0; i < 4; ++i)
            observe(SIZE_MAX, i, false, i % 2 == 0 ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED : MMAY_BYTECODE_RUN_OK, meter);
        printf("two-function JIT (%s): %zu allocation failure indices, 4 platform failure indices, exact source/fuel exit or mode rejection, zero allocation/map counters: passed\n",
            meter ? "metered" : "unmetered", allocations);
    }
    return 0;
}
