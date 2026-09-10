#include "test_bytecode_vm_support.h"
#include "vm_jit_internal.h"

typedef struct JitFixture {
    MarimayBytecodeProgram program;
    MarimayBytecodeExecutionImage image;
    MarimayBytecodeJit jit;
    MarimayFailAllocator allocations;
    MarimayCodeMemoryFault pages;
} JitFixture;

/* A false branch retains a reachable CFG backedge before the tested path.
 * Admission is asserted: interpreter fallback is not JIT coverage. */
static bool prepare(JitFixture *fixture, MarimayBytecodeFunction *function) {
    uint32_t body = UINT32_MAX, flag = UINT32_MAX, id = UINT32_MAX;
    uint64_t span = 900;
    MarimaySymbolId symbol = {{0x91}};
    memset(fixture, 0, sizeof(*fixture));
    mmay_fail_allocator_init(&fixture->allocations, mmay_system_allocator(), SIZE_MAX);
    fixture->pages.failure_index = SIZE_MAX;
    MarimayAllocator allocator = mmay_fail_allocator(&fixture->allocations);
    if (function->blocks.length != 1
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_BOOL, &flag)
        || !mmay_bytecode_add_block(function, &body)) return false;
    MarimayBytecodeBlock *blocks = function->blocks.data;
    MarimayBytecodeBlock original = blocks[0];
    blocks[0] = blocks[body];
    blocks[body] = original;
    return mmay_test_vm_append(function, 0, MMAY_OPCODE_CONST_BOOL, flag, 0, 0, &span)
        && mmay_test_vm_append(function, 0, MMAY_OPCODE_BRANCH_BOOL, flag, 0, body, &span)
        && mmay_bytecode_program_init(&fixture->program, mmay_system_allocator(),
            1, mmay_bytecode_program_default_options())
        && mmay_bytecode_program_take_function(&fixture->program, symbol, function, &id)
        && mmay_bytecode_program_set_entry(&fixture->program, symbol)
        && mmay_bytecode_program_verify(&fixture->program) == MMAY_BYTECODE_PROGRAM_VERIFY_OK
        && mmay_bytecode_execution_image_init(&fixture->image, &fixture->program,
            allocator) == MMAY_BYTECODE_IMAGE_OK
        && mmay_bytecode_jit_init(&fixture->jit, &fixture->program, &fixture->image,
            allocator, &fixture->pages, true) == MMAY_BYTECODE_RUN_OK
        && fixture->jit.ready && fixture->jit.functions[0].code != NULL;
}

static void dispose(JitFixture *fixture) {
    mmay_bytecode_jit_dispose(&fixture->jit);
    mmay_bytecode_execution_image_dispose(&fixture->image);
    mmay_bytecode_program_dispose(&fixture->program);
    MMAY_EXPECT(fixture->allocations.live_allocations == 0);
    MMAY_EXPECT(fixture->allocations.live_bytes == 0);
    MMAY_EXPECT(fixture->pages.live_mappings == 0 && fixture->pages.live_bytes == 0);
}

static bool equal_value(MarimayBytecodeValue a, MarimayBytecodeValue b) {
    return a.type == b.type && (a.type == MMAY_BYTECODE_TYPE_BOOL
        ? a.as.boolean == b.as.boolean : a.as.integer_bits == b.as.integer_bits);
}

/* Single-function dispatch is independent of the JIT-enabled program runner.
 * Compare committed results, precise source traps and both observable counters. */
static void compare(const JitFixture *fixture, const MarimayBytecodeValue *args,
    size_t count, size_t frame_limit, size_t shared_limit, bool shared) {
    const MarimayBytecodeFunction *function =
        &mmay_bytecode_program_function(&fixture->program, 0)->body;
    MarimayBytecodeRunStatus statuses[2];
    MarimayBytecodeValue values[2] = {mmay_test_vm_i64(1234), mmay_test_vm_i64(1234)};
    MarimayBytecodeTrap traps[2] = {{0}};
    size_t remaining[] = {shared_limit, shared_limit};
    for (unsigned engine = 0; engine < 2; ++engine) {
        MarimayFailAllocator allocations;
        mmay_fail_allocator_init(&allocations, mmay_system_allocator(), SIZE_MAX);
        MarimayAllocator allocator = mmay_fail_allocator(&allocations);
        MarimayBytecodeExecution execution = {0};
        MarimayBytecodeExecutionOptions memory = mmay_bytecode_execution_default_options();
        memory.managed_heap.collect_before_allocation = true;
        MMAY_EXPECT(mmay_bytecode_execution_init(&execution, allocator, memory));
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        options.execution = &execution;
        options.max_steps = frame_limit;
        options.remaining_shared_steps = shared ? &remaining[engine] : NULL;
        options.trap = &traps[engine];
        statuses[engine] = engine == 0
            ? mmay_bytecode_run(function, args, count, allocator, options, &values[engine])
            : mmay_bytecode_run_program(&fixture->program, args, count,
                allocator, options, &values[engine]);
        mmay_bytecode_execution_dispose(&execution);
        MMAY_EXPECT(allocations.live_allocations == 0 && allocations.live_bytes == 0);
    }
    MMAY_EXPECT(statuses[0] == statuses[1]);
    MMAY_EXPECT(equal_value(values[0], values[1]));
    MMAY_EXPECT(remaining[0] == remaining[1]);
    MMAY_EXPECT(traps[0].has_span == traps[1].has_span);
    if (traps[0].has_span) {
        MMAY_EXPECT(traps[0].span.start == traps[1].span.start);
        MMAY_EXPECT(traps[0].span.end == traps[1].span.end);
    }
}

static bool scalar_function(MarimayBytecodeFunction *function,
    MarimayBytecodeType type, MarimayOpcode op, bool comparison) {
    MarimayBytecodeType parameters[] = {type, type};
    MarimayBytecodeType result = comparison ? MMAY_BYTECODE_TYPE_BOOL : type;
    uint32_t target = UINT32_MAX, block = UINT32_MAX;
    uint64_t span = 10;
    return mmay_bytecode_function_init(function, mmay_system_allocator(), result,
            1, parameters, 2, mmay_bytecode_function_default_options())
        && mmay_bytecode_add_register(function, result, &target)
        && mmay_bytecode_add_block(function, &block)
        && mmay_test_vm_append(function, block, op, target, 0,
            op == MMAY_OPCODE_I64_NEGATE_CHECKED ? 0 : 1, &span)
        && mmay_test_vm_append(function, block, MMAY_OPCODE_RETURN, target, 0, 0, &span);
}

static void scalar_boundaries(void) {
    const MarimayOpcode operations[] = {
        MMAY_OPCODE_I64_ADD_CHECKED, MMAY_OPCODE_I64_SUBTRACT_CHECKED,
        MMAY_OPCODE_I64_MULTIPLY_CHECKED, MMAY_OPCODE_I64_DIVIDE_CHECKED,
        MMAY_OPCODE_I64_REMAINDER_CHECKED, MMAY_OPCODE_I64_NEGATE_CHECKED,
        MMAY_OPCODE_I64_EQUAL, MMAY_OPCODE_I64_LESS, MMAY_OPCODE_I64_LESS_EQUAL
    };
    const int64_t values[] = {INT64_MIN, INT64_MAX, -37, -1, 0, 1, 37};
    for (size_t op = 0; op < sizeof(operations) / sizeof(*operations); ++op) {
        MarimayBytecodeFunction function = {0};
        JitFixture fixture;
        MMAY_EXPECT(scalar_function(&function, MMAY_BYTECODE_TYPE_I64, operations[op], op >= 6));
        bool ready = prepare(&fixture, &function);
        MMAY_EXPECT(ready);
        if (!ready) { dispose(&fixture); mmay_bytecode_function_dispose(&function); return; }
        MMAY_EXPECT(fixture.jit.functions[0].entries[2] & MMAY_JIT_ENTRY);
        for (size_t a = 0; a < sizeof(values) / sizeof(*values); ++a) {
            for (size_t b = 0; b < sizeof(values) / sizeof(*values); ++b) {
                MarimayBytecodeValue args[] = {mmay_test_vm_i64(values[a]), mmay_test_vm_i64(values[b])};
                compare(&fixture, args, 2, SIZE_MAX, 0, false);
                compare(&fixture, args, 2, SIZE_MAX, SIZE_MAX, true);
                for (size_t fuel = 1; fuel <= 5; ++fuel) {
                    compare(&fixture, args, 2, fuel, 0, false);
                    compare(&fixture, args, 2, SIZE_MAX, fuel, true);
                    compare(&fixture, args, 2, fuel, fuel + 1, true);
                    compare(&fixture, args, 2, fuel + 1, fuel, true);
                }
            }
        }
        dispose(&fixture);
        mmay_bytecode_function_dispose(&function);
    }
}

/* I64-named wire opcodes also encode narrower/unsigned arithmetic. Verify that
 * the i64 emitter cannot bypass those types' overflow and canonical-bit rules. */
static void typed_integer_fallback(void) {
    const MarimayBytecodeType types[] = {
        MMAY_BYTECODE_TYPE_I8, MMAY_BYTECODE_TYPE_I16, MMAY_BYTECODE_TYPE_I32,
        MMAY_BYTECODE_TYPE_U8, MMAY_BYTECODE_TYPE_U16, MMAY_BYTECODE_TYPE_U32,
        MMAY_BYTECODE_TYPE_U64
    };
    const uint64_t masks[] = {255, 65535, UINT32_MAX, 255, 65535, UINT32_MAX, UINT64_MAX};
    const MarimayOpcode operations[] = {
        MMAY_OPCODE_I64_ADD_CHECKED, MMAY_OPCODE_I64_SUBTRACT_CHECKED,
        MMAY_OPCODE_I64_MULTIPLY_CHECKED, MMAY_OPCODE_I64_DIVIDE_CHECKED,
        MMAY_OPCODE_I64_REMAINDER_CHECKED, MMAY_OPCODE_I64_EQUAL,
        MMAY_OPCODE_I64_LESS, MMAY_OPCODE_I64_LESS_EQUAL, MMAY_OPCODE_I64_NEGATE_CHECKED
    };
    for (size_t type = 0; type < sizeof(types) / sizeof(*types); ++type) {
        for (size_t op = 0; op < sizeof(operations) / sizeof(*operations); ++op) {
            if (op == 8 && type >= 3) continue;
            MarimayBytecodeFunction function = {0};
            JitFixture fixture;
            MMAY_EXPECT(scalar_function(&function, types[type], operations[op], op >= 5 && op < 8));
            bool ready = prepare(&fixture, &function);
            MMAY_EXPECT(ready);
            if (!ready) { dispose(&fixture); mmay_bytecode_function_dispose(&function); return; }
            MMAY_EXPECT(!fixture.jit.functions[0].plan[2].supported);
            const uint64_t bits[] = {0, 1, masks[type], masks[type] >> 1, (masks[type] >> 1) + 1};
            for (size_t a = 0; a < sizeof(bits) / sizeof(*bits); ++a) {
                MarimayBytecodeValue args[] = {
                    {.type=types[type], .as.integer_bits=bits[a]},
                    {.type=types[type], .as.integer_bits=masks[type]}
                };
                compare(&fixture, args, 2, SIZE_MAX, 0, false);
                compare(&fixture, args, 2, 2, 1, true);
            }
            dispose(&fixture);
            mmay_bytecode_function_dispose(&function);
        }
    }
}

static bool nominal_function(MarimayBytecodeFunction *function, bool enumeration) {
    MarimayBytecodeType nominal = enumeration ? MMAY_BYTECODE_TYPE_ENUM : MMAY_BYTECODE_TYPE_RECORD;
    MarimayBytecodeType types[] = {MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64,
        nominal, MMAY_BYTECODE_TYPE_I64, nominal};
    MarimaySymbolId symbol = {{0x92}};
    uint32_t layout = UINT32_MAX, construction = UINT32_MAX, block = UINT32_MAX;
    uint64_t span = 100;
    if (!mmay_bytecode_function_init(function, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_I64, 1, NULL, 0, mmay_bytecode_function_default_options())
        || !mmay_test_vm_add_registers(function, types, 5)) return false;
    if (enumeration) {
        MarimayBytecodeEnumVariant variants[] = {{0, 1}, {1, 1}};
        MarimayBytecodeEnumPayload payloads[] = {
            {MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_NO_LAYOUT},
            {MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_NO_LAYOUT}
        };
        MarimayBytecodeEnumArgument argument = {1, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_NO_LAYOUT};
        if (!mmay_bytecode_add_enum_layout(function, symbol, variants, 2, payloads, 2, &layout)
            || !mmay_bytecode_add_enum_construction(function, layout, 1, &argument, 1, &construction)) return false;
    } else {
        MarimayBytecodeRecordField fields[] = {
            {.type=MMAY_BYTECODE_TYPE_I64, .layout=MMAY_BYTECODE_NO_LAYOUT},
            {.type=MMAY_BYTECODE_TYPE_I64, .layout=MMAY_BYTECODE_NO_LAYOUT}
        };
        MarimayBytecodeRecordArgument arguments[] = {
            {0, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_NO_LAYOUT},
            {1, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_NO_LAYOUT}
        };
        if (!mmay_bytecode_add_record_layout(function, symbol, fields, 2, &layout)
            || !mmay_bytecode_add_record_construction(function, layout, arguments, 2, &construction)) return false;
    }
    if (!mmay_bytecode_set_register_layout(function, 2, layout)
        || !mmay_bytecode_set_register_layout(function, 4, layout)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_CONST_I64, 0, 17, 0, &span)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_CONST_I64, 1, 42, 0, &span)
        || !mmay_test_vm_append(function, block,
            enumeration ? MMAY_OPCODE_ENUM_NEW : MMAY_OPCODE_RECORD_NEW, 2, construction, 0, &span)) return false;
    if (enumeration) {
        if (!mmay_test_vm_append(function, block, MMAY_OPCODE_ENUM_REQUIRE_VARIANT, 4, 2, 0, &span)) return false;
        mmay_test_vm_at(function, 0, 3)->operands[2].signed_value = 1;
    }
    size_t projection = enumeration ? 4 : 3;
    if (!mmay_test_vm_append(function, block,
            enumeration ? MMAY_OPCODE_ENUM_GET_PAYLOAD : MMAY_OPCODE_RECORD_GET,
            3, enumeration ? 4 : 2, 0, &span)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_RETURN, 3, 0, 0, &span)) return false;
    /* The signed field is authoritative. In particular, variant 1's packed
     * selector exceeds the compact image's 32-bit third operand. */
    mmay_test_vm_at(function, 0, projection)->operands[2].signed_value =
        enumeration ? INT64_C(1) << 32 : 1;
    return true;
}

static void nominal_selectors(void) {
    for (unsigned enumeration = 0; enumeration < 2; ++enumeration) {
        MarimayBytecodeFunction function = {0};
        JitFixture fixture;
        MMAY_EXPECT(nominal_function(&function, enumeration != 0));
        bool ready = prepare(&fixture, &function);
        MMAY_EXPECT(ready);
        if (!ready) { dispose(&fixture); mmay_bytecode_function_dispose(&function); return; }
        MMAY_EXPECT(fixture.jit.functions[0].entries[5] & MMAY_JIT_ENTRY);
        MMAY_EXPECT(fixture.jit.functions[0].plan[enumeration ? 6 : 5].supported);
        MarimayBytecodeValue result = {0};
        MarimayBytecodeExecution execution = {0};
        MMAY_EXPECT(mmay_bytecode_execution_init(&execution, mmay_system_allocator(),
            mmay_bytecode_execution_default_options()));
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        options.execution = &execution;
        MMAY_EXPECT(mmay_bytecode_run_program(&fixture.program, NULL, 0,
            mmay_system_allocator(), options, &result) == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 42);
        mmay_bytecode_execution_dispose(&execution);
        compare(&fixture, NULL, 0, SIZE_MAX, 0, false);
        for (size_t fuel = 1; fuel <= 10; ++fuel) {
            compare(&fixture, NULL, 0, fuel, 0, false);
            compare(&fixture, NULL, 0, SIZE_MAX, fuel, true);
            compare(&fixture, NULL, 0, fuel, fuel + 1, true);
        }
        dispose(&fixture);
        mmay_bytecode_function_dispose(&function);
    }
}

void mmay_test_vm_jit_semantics(void) {
    if (!mmay_code_memory_supported()) return;
    scalar_boundaries();
    typed_integer_fallback();
    nominal_selectors();
}
