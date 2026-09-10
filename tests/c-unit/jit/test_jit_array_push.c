#include "vm_jit_internal.h"
#include "vm_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "JIT scalar push: %s at %d\n", #c, __LINE__); exit(1); } } while (0)

typedef struct Fixture {
    MarimayBytecodeProgram program;
    MarimayBytecodeType element;
    uint64_t bits;
} Fixture;

static void append(MarimayBytecodeFunction *f, uint32_t block, MarimayOpcode op,
    uint64_t a, uint64_t b, uint64_t c) {
    MarimayBytecodeInstruction instruction = {.opcode=op,
        .span={.start=f->instruction_count + 100, .end=f->instruction_count + 101}};
    instruction.operands[0].unsigned_value = a;
    instruction.operands[1].unsigned_value = b;
    instruction.operands[2].unsigned_value = c;
    EXPECT(mmay_bytecode_append_instruction(f, block, &instruction));
}

static void fixture_init(Fixture *fixture, MarimayBytecodeType type, uint64_t bits) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->element = type; fixture->bits = bits;
    MarimayAllocator a = mmay_system_allocator();
    MarimayBytecodeFunction f = {0};
    MarimayBytecodeType params[] = {MMAY_BYTECODE_TYPE_ARRAY, type, MMAY_BYTECODE_TYPE_I64};
    uint32_t added;
    EXPECT(mmay_bytecode_program_init(&fixture->program, a, 1, mmay_bytecode_program_default_options()));
    EXPECT(mmay_bytecode_function_init(&f, a, MMAY_BYTECODE_TYPE_I64, 1, params, 3,
        mmay_bytecode_function_default_options()));
    EXPECT(mmay_bytecode_add_array_type(&f, type, MMAY_BYTECODE_NO_LAYOUT, &added));
    EXPECT(added == 0 && mmay_bytecode_set_register_layout(&f, 0, added));
    EXPECT(mmay_bytecode_add_register(&f, MMAY_BYTECODE_TYPE_BOOL, &added) && added == 3);
    EXPECT(mmay_bytecode_add_register(&f, MMAY_BYTECODE_TYPE_I64, &added) && added == 4);
    EXPECT(mmay_bytecode_add_register(&f, type, &added) && added == 5);
    EXPECT(mmay_bytecode_add_block(&f, &added) && added == 0);
    EXPECT(mmay_bytecode_add_block(&f, &added) && added == 1);
    append(&f, 0, MMAY_OPCODE_CONST_BOOL, 3, 0, 0);
    append(&f, 0, MMAY_OPCODE_BRANCH_BOOL, 3, 0, 1);
    append(&f, 1, MMAY_OPCODE_ARRAY_PUSH, 0, 1, 0);
    append(&f, 1, MMAY_OPCODE_ARRAY_PUSH, 0, 1, 0);
    append(&f, 1, MMAY_OPCODE_ARRAY_LENGTH, 4, 0, 0);
    append(&f, 1, MMAY_OPCODE_ARRAY_GET, 5, 0, 2);
    append(&f, 1, MMAY_OPCODE_RETURN, 4, 0, 0);
    MarimaySymbolId symbol = {{0x31}};
    EXPECT(mmay_bytecode_program_take_function(&fixture->program, symbol, &f, &added));
    EXPECT(mmay_bytecode_program_set_entry(&fixture->program, symbol));
    EXPECT(mmay_bytecode_program_verify(&fixture->program) == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}

static const MarimayBytecodeFunction *body(const Fixture *f) {
    return &mmay_bytecode_program_function(&f->program, 0)->body;
}

/* Capacity 0 forces ordinary growth; capacity 4 admits both native pushes.
 * Modes 1/2 make generation or configured length reject the second push. */
static void setup(const Fixture *f, MarimayBytecodeExecution *e, MarimayAllocator a,
    MarimayBytecodeValue args[3], size_t capacity, size_t mode, MarimayManagedArrayView *view) {
    EXPECT(mmay_bytecode_execution_init(e, a, mmay_bytecode_execution_default_options()));
    const MarimayManagedArrayDescriptor *d = mmay_bytecode_array_descriptor_for(e, body(f), MMAY_BYTECODE_TYPE_ARRAY, 0);
    EXPECT(d != NULL);
    args[0].type = MMAY_BYTECODE_TYPE_ARRAY;
    args[1] = (MarimayBytecodeValue){.type=f->element, .as.integer_bits=f->bits};
    if (f->element == MMAY_BYTECODE_TYPE_BOOL) args[1].as.boolean = f->bits != 0;
    args[2] = (MarimayBytecodeValue){.type=MMAY_BYTECODE_TYPE_I64, .as.i64=0};
    EXPECT(mmay_managed_array_new(&e->managed_heap, d, NULL, 0, &args[0].as.managed_reference) == MMAY_MANAGED_OK);
    EXPECT(mmay_managed_array_reserve(&e->managed_heap, args[0].as.managed_reference, (int64_t)capacity) == MMAY_MANAGED_OK);
    EXPECT(mmay_managed_array_view_bind(&e->managed_heap, args[0].as.managed_reference, d, view) == MMAY_MANAGED_OK);
    if (mode == 1) *view->mutation_generation = UINT64_MAX - 1;
    if (mode == 2) e->managed_heap.options.max_array_length = 1;
    e->managed_heap.options.collect_before_allocation = true;
    e->managed_heap.last_resource_failure = (MarimayManagedResourceFailure){
        .classification=MMAY_MANAGED_RESOURCE_FAILURE_BUDGET, .requested_bytes=77};
}

typedef struct Observation {
    MarimayBytecodeRunStatus status;
    MarimayBytecodeTrap trap;
    int64_t result;
    size_t length, remaining, attempts;
    uint64_t generation, cells[2];
    MarimayManagedResourceFailure failure;
} Observation;

static Observation run(const Fixture *f, bool jit, size_t capacity, size_t mode,
    size_t fuel, bool shared, size_t failure) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator a = mmay_fail_allocator(&failing);
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeValue args[3], result = {.type=MMAY_BYTECODE_TYPE_I64, .as.i64=1234};
    MarimayManagedArrayView view;
    setup(f, &execution, a, args, capacity, mode, &view);
    Observation o = {.remaining=fuel};
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.execution = &execution; options.trap = &o.trap;
    options.max_steps = shared ? SIZE_MAX : fuel;
    options.remaining_shared_steps = shared ? &o.remaining : NULL;
    failing.attempts = 0; failing.failure_index = failure;
    o.status = jit ? mmay_bytecode_run_program(&f->program, args, 3, a, options, &result)
        : mmay_bytecode_run(body(f), args, 3, a, options, &result);
    o.attempts = failing.attempts; o.result = result.as.i64;
    if (failure < failing.attempts) EXPECT(o.status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED && o.result == 1234);
    o.length = *view.length; o.generation = *view.mutation_generation;
    o.failure = execution.managed_heap.last_resource_failure;
    EXPECT(o.length <= 2 && execution.managed_heap.roots == NULL);
    for (size_t i = 0; i < o.length; ++i) {
        MarimayManagedCell cell;
        size_t length;
        EXPECT(mmay_managed_array_get(&execution.managed_heap, args[0].as.managed_reference,
            (int64_t)i, &cell, &length) == MMAY_MANAGED_OK);
        o.cells[i] = cell.f64_bits;
        EXPECT(cell.f64_bits == f->bits);
    }
    mmay_bytecode_execution_dispose(&execution);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    return o;
}

static void compare(const Fixture *f, size_t capacity, size_t mode, size_t fuel, bool shared) {
    Observation a = run(f, false, capacity, mode, fuel, shared, SIZE_MAX);
    Observation b = run(f, true, capacity, mode, fuel, shared, SIZE_MAX);
    EXPECT(a.status == b.status && a.result == b.result && a.length == b.length
        && a.remaining == b.remaining && a.generation == b.generation);
    EXPECT(a.trap.has_span == b.trap.has_span);
    if (a.trap.has_span) EXPECT(a.trap.span.start == b.trap.span.start && a.trap.span.end == b.trap.span.end);
    EXPECT(a.failure.classification == b.failure.classification
        && a.failure.live_bytes == b.failure.live_bytes
        && a.failure.requested_bytes == b.failure.requested_bytes
        && a.failure.limit_bytes == b.failure.limit_bytes);
    if (a.status == MMAY_BYTECODE_RUN_OK) EXPECT(a.result == 2 && a.length == 2 && a.generation == 2);
}

static void native(const Fixture *f, size_t mode) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator a = mmay_fail_allocator(&failing);
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeValue regs[6] = {0};
    MarimayManagedArrayView view;
    setup(f, &execution, a, regs, mode == 3 ? 0 : 4, mode, &view);
    regs[3].type = MMAY_BYTECODE_TYPE_BOOL; regs[4].type = MMAY_BYTECODE_TYPE_I64;
    regs[5].type = f->element;
    MarimayCodeMemoryFault pages = {.failure_index=SIZE_MAX};
    MarimayBytecodeExecutionImage image = {0};
    MarimayBytecodeJit jit = {0};
    EXPECT(mmay_bytecode_execution_image_init(&image, &f->program, a) == MMAY_BYTECODE_IMAGE_OK);
    EXPECT(mmay_bytecode_jit_init(&jit, &f->program, &image, a, &pages, true) == MMAY_BYTECODE_RUN_OK);
    EXPECT(jit.ready && jit.functions[0].code != NULL && jit.functions[0].plan[2].supported);
    size_t block = 1, pc = 0, steps = 0, budget = 4;
    size_t attempts = failing.attempts, live_bytes = execution.managed_heap.live_byte_count;
    uint64_t epoch = execution.managed_heap.view_epoch;
    failing.failure_index = attempts;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.execution = &execution; options.max_steps = SIZE_MAX; options.remaining_shared_steps = &budget;
    EXPECT(mmay_bytecode_jit_run(&jit, 0, &image.functions[0], body(f), regs,
        &execution, &block, &pc, &steps, options));
    EXPECT(failing.attempts == attempts && execution.managed_heap.live_byte_count == live_bytes
        && execution.managed_heap.view_epoch == epoch && block == 1);
    EXPECT(pc == (mode == 0 ? 4 : mode == 3 ? 0 : 1));
    EXPECT(*view.length == (mode == 0 ? 2 : mode == 3 ? 0 : 1));
    EXPECT(budget == (mode == 0 ? 0 : mode == 3 ? 4 : 3));
    if (mode != 3) EXPECT(execution.managed_heap.last_resource_failure.classification == MMAY_MANAGED_RESOURCE_FAILURE_NONE);
    mmay_bytecode_jit_dispose(&jit);
    mmay_bytecode_execution_image_dispose(&image);
    mmay_bytecode_execution_dispose(&execution);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    EXPECT(pages.live_mappings == 0 && pages.live_bytes == 0);
}

int main(void) {
    if (!mmay_code_memory_supported()) return 77;
    const struct { MarimayBytecodeType type; uint64_t bits; } cases[] = {
        {MMAY_BYTECODE_TYPE_BOOL, 1}, {MMAY_BYTECODE_TYPE_I8, 255}, {MMAY_BYTECODE_TYPE_U8, 255},
        {MMAY_BYTECODE_TYPE_I16, 65535}, {MMAY_BYTECODE_TYPE_U16, 65535},
        {MMAY_BYTECODE_TYPE_I32, UINT32_MAX}, {MMAY_BYTECODE_TYPE_U32, UINT32_MAX},
        {MMAY_BYTECODE_TYPE_I64, UINT64_MAX}, {MMAY_BYTECODE_TYPE_U64, UINT64_MAX},
        {MMAY_BYTECODE_TYPE_RUNE, 0x10ffff}, {MMAY_BYTECODE_TYPE_F32, UINT32_C(0x80000000)},
        {MMAY_BYTECODE_TYPE_F64, UINT64_C(0x8000000000000000)}
    };
    for (size_t type = 0; type < sizeof(cases) / sizeof(*cases); ++type) {
        Fixture f;
        fixture_init(&f, cases[type].type, cases[type].bits);
        for (size_t mode = 0; mode < 4; ++mode) native(&f, mode);
        for (size_t mode = 0; mode < 3; ++mode) for (size_t cap = 0; cap < 2; ++cap)
            for (size_t shared = 0; shared < 2; ++shared) {
                compare(&f, cap * 4, mode, SIZE_MAX, shared != 0);
                for (size_t fuel = 1; fuel <= 9; ++fuel) compare(&f, cap * 4, mode, fuel, shared != 0);
            }
        for (size_t meter = 0; meter < 2; ++meter) for (size_t cap = 0; cap < 2; ++cap) {
            size_t count = run(&f, true, cap * 4, 0, meter ? 20 : SIZE_MAX, false, SIZE_MAX).attempts;
            for (size_t i = 0; i <= count; ++i) run(&f, true, cap * 4, 0, meter ? 20 : SIZE_MAX, false, i);
        }
        mmay_bytecode_program_dispose(&f.program);
    }
    puts("JIT scalar pushes: all storage widths, actual allocation-free native execution, growth exits, length/generation limits, resource diagnostics, exact shared fuel and allocation failures: passed");
    return 0;
}
