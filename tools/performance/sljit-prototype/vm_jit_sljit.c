/* Experimental adapter, outside production: old VM region contract + SLJIT. */
#include "vm_jit_internal.h"
#include "bytecode_internal.h"
#include "sljitLir.h"
#include "jit_memory.h"
#include <string.h>

#define CTX SLJIT_S0
#define REGS SLJIT_S1
#define BINDS SLJIT_S2
#define FUEL SLJIT_S3
#define EPOCH SLJIT_S4

typedef struct Emitter {
    struct sljit_compiler *c;
    const MarimayBytecodeFunction *function;
    size_t pc;
    size_t restore;
} Emitter;

static sljit_sw slot(uint32_t reg) {
    return (sljit_sw)(reg * sizeof(MarimayBytecodeValue) + offsetof(MarimayBytecodeValue, as));
}
static void move(Emitter *e, int op, int dst, sljit_sw dw, int src, sljit_sw sw) {
    sljit_emit_op1(e->c, op, dst, dw, src, sw);
}
static void load(Emitter *e, int target, uint32_t reg) {
    const MarimayBytecodeType *types = e->function->register_types.data;
    move(e, types[reg] == MMAY_BYTECODE_TYPE_BOOL ? SLJIT_MOV_U8 : SLJIT_MOV,
        target, 0, SLJIT_MEM1(REGS), slot(reg));
}
static void save(Emitter *e, uint32_t reg, int source) {
    const MarimayBytecodeType *types = e->function->register_types.data;
    move(e, types[reg] == MMAY_BYTECODE_TYPE_BOOL ? SLJIT_MOV_U8 : SLJIT_MOV,
        SLJIT_MEM1(REGS), slot(reg), source, 0);
}
static void invalidate(Emitter *e, uint32_t reg) {
    const MarimayBytecodeType *types = e->function->register_types.data;
    if (!mmay_bytecode_type_is_array(types[reg]) && types[reg] != MMAY_BYTECODE_TYPE_RECORD) return;
    sljit_sw base = (sljit_sw)(reg * sizeof(MarimayBytecodeJitBinding));
    _Static_assert(offsetof(MarimayManagedArrayView, reference) == 0 && offsetof(MarimayManagedRecordView, reference) == 0, "view identity prefix");
    move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(BINDS), base + offsetof(MarimayBytecodeJitBinding, view));
    struct sljit_jump *different = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_MEM1(REGS), slot(reg));
    move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(BINDS), base + offsetof(MarimayBytecodeJitBinding, view) + 8);
    struct sljit_jump *same = sljit_emit_cmp(e->c, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_MEM1(REGS), slot(reg) + 8);
    sljit_set_label(different, sljit_emit_label(e->c));
    move(e, SLJIT_MOV, SLJIT_MEM1(BINDS), base, SLJIT_IMM, 0);
    sljit_set_label(same, sljit_emit_label(e->c));
}
static void copy(Emitter *e, uint32_t dst, uint32_t src) {
    const MarimayBytecodeType *types = e->function->register_types.data;
    size_t count = mmay_bytecode_type_is_managed(types[dst]) ? sizeof(MarimayManagedReference)/8 : 1;
    int op = types[dst] == MMAY_BYTECODE_TYPE_BOOL ? SLJIT_MOV_U8 : SLJIT_MOV;
    for (size_t i = 0; i < count; ++i) {
        move(e, op, SLJIT_R0, 0, SLJIT_MEM1(REGS), slot(src) + (sljit_sw)(8*i));
        move(e, op, SLJIT_MEM1(REGS), slot(dst) + (sljit_sw)(8*i), SLJIT_R0, 0);
    }
    invalidate(e, dst);
}
static void leave(Emitter *e) {
    if (e->restore) sljit_emit_op2(e->c, SLJIT_ADD, FUEL, 0, FUEL, 0, SLJIT_IMM, (sljit_sw)e->restore);
    move(e, SLJIT_MOV, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, budget), FUEL, 0);
    move(e, SLJIT_MOV, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, pc), SLJIT_IMM, (sljit_sw)e->pc);
    sljit_emit_return_void(e->c);
}
static void guard(Emitter *e, struct sljit_jump *success) {
    leave(e);
    sljit_set_label(success, sljit_emit_label(e->c));
}
static void condition(Emitter *e, int kind, int left, sljit_sw lw, int right, sljit_sw rw) {
    guard(e, sljit_emit_cmp(e->c, kind, left, lw, right, rw));
}
static void boolean(Emitter *e, int condition_type, int left, sljit_sw lw, int right, sljit_sw rw) {
    int flags = condition_type == SLJIT_EQUAL || condition_type == SLJIT_NOT_EQUAL ? SLJIT_SET_Z : SLJIT_SET(condition_type & ~1);
    sljit_emit_op2u(e->c, SLJIT_SUB | flags, left, lw, right, rw);
    sljit_emit_op_flags(e->c, SLJIT_MOV, SLJIT_R0, 0, condition_type);
}
static sljit_sw bind_address(void) {
    void *(*function)(MarimayBytecodeJitContext *, uint32_t) = mmay_bytecode_jit_bind;
    sljit_sw address = 0;
    _Static_assert(sizeof(address) == sizeof(function), "helper pointer");
    memcpy(&address, &function, sizeof(address));
    return address;
}
static sljit_sw enum_address(void) {
    bool (*function)(MarimayBytecodeJitContext *, uint32_t) = mmay_bytecode_jit_enum;
    sljit_sw address = 0;
    memcpy(&address, &function, sizeof(address));
    return address;
}
static void composed_shift(Emitter *e) {
    bool (*function)(MarimayBytecodeJitContext *, uint32_t) = mmay_bytecode_jit_shift;
    sljit_sw address = 0;
    _Static_assert(sizeof(address) == sizeof(function), "helper pointer");
    memcpy(&address, &function, sizeof(address));
    move(e, SLJIT_MOV, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, budget), FUEL, 0);
    move(e, SLJIT_MOV, SLJIT_R0, 0, CTX, 0);
    move(e, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)e->pc);
    sljit_emit_icall(e->c, SLJIT_CALL, SLJIT_ARGS2(32, P, 32), SLJIT_IMM, address);
    struct sljit_jump *unchanged = sljit_emit_cmp(e->c, SLJIT_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 0);
    sljit_emit_return_void(e->c);
    sljit_set_label(unchanged, sljit_emit_label(e->c));
}
/* Returns view in R5. No borrowed data address survives a VM safepoint. */
static void bind(Emitter *e, uint32_t reg) {
    sljit_sw base = (sljit_sw)(reg * sizeof(MarimayBytecodeJitBinding));
    struct sljit_jump *hit = sljit_emit_cmp(e->c, SLJIT_EQUAL,
        SLJIT_MEM1(BINDS), base, EPOCH, 0);
    move(e, SLJIT_MOV, SLJIT_R0, 0, CTX, 0);
    move(e, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reg);
    sljit_emit_icall(e->c, SLJIT_CALL, SLJIT_ARGS2(P, P, 32), SLJIT_IMM, bind_address());
    condition(e, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    sljit_set_label(hit, sljit_emit_label(e->c));
    sljit_emit_op2(e->c, SLJIT_ADD, SLJIT_R5, 0, BINDS, 0, SLJIT_IMM,
        base + offsetof(MarimayBytecodeJitBinding, view));
}
static void array(Emitter *e, const MarimayBytecodeImageOperation *op) {
    bool store = op->opcode == MMAY_OPCODE_ARRAY_SET || op->opcode == MMAY_OPCODE_ARRAY_SET_IN_BOUNDS;
    uint32_t source = store ? op->first : op->second;
    bind(e, source);
    if (op->opcode == MMAY_OPCODE_ARRAY_LENGTH) {
        move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R5), offsetof(MarimayManagedArrayView, length));
        move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), 0);
        condition(e, SLJIT_SIG_GREATER_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        save(e, op->first, SLJIT_R0); return;
    }
    if (op->opcode == MMAY_OPCODE_ARRAY_GENERATION || op->opcode == MMAY_OPCODE_ARRAY_REQUIRE_GENERATION) {
        move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R5), offsetof(MarimayManagedArrayView, mutation_generation));
        move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), 0);
        if (op->opcode == MMAY_OPCODE_ARRAY_GENERATION) save(e, op->first, SLJIT_R0);
        else {
            load(e, SLJIT_R1, op->third);
            condition(e, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0);
            copy(e, op->first, op->second);
        }
        return;
    }
    load(e, SLJIT_R1, store ? op->second : op->third);
    if (op->opcode != MMAY_OPCODE_ARRAY_GET_IN_BOUNDS && op->opcode != MMAY_OPCODE_ARRAY_SET_IN_BOUNDS) {
        condition(e, SLJIT_SIG_GREATER_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
        move(e, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R5), offsetof(MarimayManagedArrayView, length));
        condition(e, SLJIT_LESS, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R2), 0);
    }
    if (op->opcode == MMAY_OPCODE_ARRAY_REQUIRE_INDEX) { copy(e, op->first, op->second); return; }
    size_t width = mmay_bytecode_jit_array_width(e->function, source);
    int shift = width == 8 ? 3 : width == 4 ? 2 : width == 2 ? 1 : 0;
    int mov = width == 8 ? SLJIT_MOV : width == 4 ? SLJIT_MOV_U32 : width == 2 ? SLJIT_MOV_U16 : SLJIT_MOV_U8;
    if (store) {
        load(e, SLJIT_R0, op->third);
        if (width < 8) condition(e, SLJIT_LESS_EQUAL, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)((UINT64_C(1) << (width * 8)) - 1));
        move(e, SLJIT_MOV, SLJIT_R3, 0, SLJIT_MEM1(SLJIT_R5), offsetof(MarimayManagedArrayView, mutation_generation));
        move(e, SLJIT_MOV, SLJIT_R4, 0, SLJIT_MEM1(SLJIT_R3), 0);
        condition(e, SLJIT_NOT_EQUAL, SLJIT_R4, 0, SLJIT_IMM, -1);
    }
    move(e, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R5), offsetof(MarimayManagedArrayView, storage));
    move(e, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R2), 0);
    if (store) {
        move(e, mov, SLJIT_MEM2(SLJIT_R2, SLJIT_R1), shift, SLJIT_R0, 0);
        sljit_emit_op2(e->c, SLJIT_ADD, SLJIT_R4, 0, SLJIT_R4, 0, SLJIT_IMM, 1);
        move(e, SLJIT_MOV, SLJIT_MEM1(SLJIT_R3), 0, SLJIT_R4, 0);
    } else {
        move(e, mov, SLJIT_R0, 0, SLJIT_MEM2(SLJIT_R2, SLJIT_R1), shift);
        const MarimayBytecodeType *types = e->function->register_types.data;
        if (types[op->first] == MMAY_BYTECODE_TYPE_BOOL) boolean(e, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        save(e, op->first, SLJIT_R0);
    }
}
static void record(Emitter *e, const MarimayBytecodeImageOperation *op) {
    bind(e, op->second);
    move(e, SLJIT_MOV, SLJIT_R5, 0, SLJIT_MEM1(SLJIT_R5), offsetof(MarimayManagedRecordView, fields));
    const MarimayBytecodeType *types = e->function->register_types.data;
    bool managed = mmay_bytecode_type_is_managed(types[op->first]);
    size_t count = managed ? sizeof(MarimayManagedReference) / 8 : 1;
    for (size_t part = 0; part < count; ++part) {
        move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R5),
            (sljit_sw)(op->third * sizeof(MarimayManagedCell) + part * 8));
        if (!managed && types[op->first] == MMAY_BYTECODE_TYPE_BOOL)
            boolean(e, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        move(e, types[op->first] == MMAY_BYTECODE_TYPE_BOOL ? SLJIT_MOV_U8 : SLJIT_MOV,
            SLJIT_MEM1(REGS), slot(op->first) + (sljit_sw)(part * 8), SLJIT_R0, 0);
    }
    invalidate(e, op->first);
}
static void scalar(Emitter *e, const MarimayBytecodeImageOperation *op) {
    if (op->opcode == MMAY_OPCODE_COPY) { copy(e, op->first, op->second); return; }
    if (op->opcode == MMAY_OPCODE_CONST_I64 || op->opcode == MMAY_OPCODE_CONST_INTEGER
        || op->opcode == MMAY_OPCODE_CONST_F64 || op->opcode == MMAY_OPCODE_CONST_BOOL) {
        sljit_sw bits; memcpy(&bits, &op->immediate, sizeof(bits));
        move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, bits);
        save(e, op->first, SLJIT_R0); return;
    }
    load(e, SLJIT_R0, op->second);
    if (op->opcode == MMAY_OPCODE_BOOL_NOT) {
        boolean(e, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    } else if (op->opcode == MMAY_OPCODE_I64_NEGATE_CHECKED) {
        sljit_emit_op2(e->c, SLJIT_SUB | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_IMM, 0, SLJIT_R0, 0);
        guard(e, sljit_emit_jump(e->c, SLJIT_NOT_OVERFLOW));
    } else {
        load(e, SLJIT_R1, op->third);
        int kind = 0;
        bool checked = false;
        switch (op->opcode) {
            case MMAY_OPCODE_I64_ADD_CHECKED: checked = true; /* fall through */
            case MMAY_OPCODE_I64_ADD_PROVEN: kind = SLJIT_ADD; break;
            case MMAY_OPCODE_I64_SUBTRACT_CHECKED: checked = true; /* fall through */
            case MMAY_OPCODE_I64_SUBTRACT_PROVEN: kind = SLJIT_SUB; break;
            case MMAY_OPCODE_I64_MULTIPLY_CHECKED: checked = true; /* fall through */
            case MMAY_OPCODE_I64_MULTIPLY_PROVEN: kind = SLJIT_MUL; break;
            case MMAY_OPCODE_I64_DIVIDE_CHECKED:
            case MMAY_OPCODE_I64_REMAINDER_CHECKED: {
                condition(e, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
                struct sljit_jump *safe = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, -1);
                condition(e, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, INTPTR_MIN);
                sljit_set_label(safe, sljit_emit_label(e->c));
                sljit_emit_op0(e->c, SLJIT_DIVMOD_SW);
                if (op->opcode == MMAY_OPCODE_I64_REMAINDER_CHECKED) move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_R1, 0);
                break;
            }
            default:
                boolean(e, op->opcode == MMAY_OPCODE_I64_LESS ? SLJIT_SIG_LESS
                    : op->opcode == MMAY_OPCODE_I64_LESS_EQUAL ? SLJIT_SIG_LESS_EQUAL : SLJIT_EQUAL,
                    SLJIT_R0, 0, SLJIT_R1, 0); break;
        }
        if (kind) {
            sljit_emit_op2(e->c, kind | (checked ? SLJIT_SET_OVERFLOW : 0), SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
            if (checked) guard(e, sljit_emit_jump(e->c, SLJIT_NOT_OVERFLOW));
        }
    }
    save(e, op->first, SLJIT_R0);
}

MarimayCodeMemoryStatus mmay_bytecode_jit_sljit(const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeJitFunction *compiled,
    MarimayAllocator *allocator, size_t capacity, MarimayCodeMemoryFault *fault) {
    size_t n = image->operation_count;
    size_t scratch_bytes = n * (sizeof(struct sljit_label *) + 2*sizeof(struct sljit_jump *));
    void *scratch = mmay_allocate(allocator, scratch_bytes);
    if (!scratch) return MMAY_CODE_MEMORY_ALLOCATION_FAILED;
    memset(scratch, 0, scratch_bytes);
    struct sljit_label **labels = scratch;
    struct sljit_jump **edges = (struct sljit_jump **)(labels + n);
    struct sljit_compiler *compiler = sljit_create_compiler(allocator);
    if (!compiler) { mmay_deallocate(allocator, scratch, scratch_bytes); return MMAY_CODE_MEMORY_ALLOCATION_FAILED; }
    Emitter e = {compiler, function, 0, 0};
    sljit_emit_enter(compiler, 0, SLJIT_ARGS2V(P, P), 6, 5, 0);
    move(&e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S1, 0);
    move(&e, SLJIT_MOV, REGS, 0, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, registers));
    move(&e, SLJIT_MOV, BINDS, 0, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, bindings));
    move(&e, SLJIT_MOV, FUEL, 0, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, budget));
    move(&e, SLJIT_MOV, EPOCH, 0, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, epoch));
    sljit_emit_ijump(compiler, SLJIT_JUMP, SLJIT_R0, 0);
    for (size_t pc = 0; pc < n; ++pc) {
        const MarimayBytecodeImageOperation *op = &image->operations[pc];
        const MarimayBytecodeJitPlanOperation *plan = &compiled->plan[pc];
        labels[pc] = sljit_emit_label(compiler);
        e.pc = pc; e.restore = 0;
        if (!plan->supported) { leave(&e); continue; }
        if (plan->starts_segment && op->fusion == MMAY_BYTECODE_IMAGE_FUSION_ARRAY_SHIFT_LOOP)
            composed_shift(&e);
        if (plan->starts_segment) {
            condition(&e, SLJIT_GREATER_EQUAL, FUEL, 0, SLJIT_IMM, plan->segment_end - pc);
            sljit_emit_op2(compiler, SLJIT_SUB, FUEL, 0, FUEL, 0, SLJIT_IMM, plan->segment_end - pc);
        }
        e.restore = plan->segment_end - pc;
        switch (op->opcode) {
            case MMAY_OPCODE_JUMP: edges[2*pc] = sljit_emit_jump(compiler, SLJIT_JUMP); break;
            case MMAY_OPCODE_BRANCH_BOOL:
                load(&e, SLJIT_R0, op->first);
                edges[2*pc] = sljit_emit_cmp(compiler, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
                edges[2*pc+1] = sljit_emit_jump(compiler, SLJIT_JUMP); break;
            case MMAY_OPCODE_ARRAY_LENGTH: case MMAY_OPCODE_ARRAY_GENERATION:
            case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION: case MMAY_OPCODE_ARRAY_REQUIRE_INDEX:
            case MMAY_OPCODE_ARRAY_GET: case MMAY_OPCODE_ARRAY_SET:
            case MMAY_OPCODE_ARRAY_GET_IN_BOUNDS: case MMAY_OPCODE_ARRAY_SET_IN_BOUNDS:
                array(&e, op); break;
            case MMAY_OPCODE_RECORD_GET: record(&e, op); break;
            case MMAY_OPCODE_ENUM_IS_VARIANT: case MMAY_OPCODE_ENUM_REQUIRE_VARIANT:
            case MMAY_OPCODE_ENUM_GET_PAYLOAD:
                move(&e, SLJIT_MOV, SLJIT_R0, 0, CTX, 0);
                move(&e, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)pc);
                sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(32, P, 32), SLJIT_IMM, enum_address());
                condition(&e, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 0);
                invalidate(&e, op->first); break;
            default: scalar(&e, op); break;
        }
    }
    for (size_t pc = 0; pc < n; ++pc) {
        const MarimayBytecodeImageOperation *op = &image->operations[pc];
        if (edges[2*pc]) sljit_set_label(edges[2*pc], labels[op->opcode == MMAY_OPCODE_JUMP ? op->first : op->second]);
        if (edges[2*pc+1]) sljit_set_label(edges[2*pc+1], labels[op->third]);
    }
    MarimayJitCodeBuild build = {.allocator = *allocator, .capacity = capacity, .status = MMAY_CODE_MEMORY_OK, .fault = fault};
    void *code = sljit_generate_code(compiler, 0, &build);
    size_t bytes = sljit_get_generated_code_size(compiler);
    MarimayCodeMemoryStatus status = code ? mmay_jit_code_publish(&build, code, bytes)
        : build.status != MMAY_CODE_MEMORY_OK ? build.status : MMAY_CODE_MEMORY_ALLOCATION_FAILED;
    if (status == MMAY_CODE_MEMORY_OK) {
        compiled->memory = build.memory;
        memset(&build.memory, 0, sizeof(build.memory));
        compiled->code = code;
        compiled->code_bytes = bytes;
        for (size_t pc = 0; pc < n; ++pc) {
            uintptr_t address = sljit_get_label_addr(labels[pc]);
            compiled->entries[pc] = (uint32_t)(address - (uintptr_t)code)
                | (compiled->plan[pc].starts_segment ? MMAY_JIT_ENTRY : 0);
        }
    }
    mmay_jit_code_dispose(&build);
    sljit_free_compiler(compiler);
    mmay_deallocate(allocator, scratch, scratch_bytes);
    return status;
}
