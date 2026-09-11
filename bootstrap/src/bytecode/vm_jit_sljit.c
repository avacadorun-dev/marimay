/* Machine-code lowering for verified nonallocating VM segments. */
#include "vm_jit_internal.h"
#include "vm_jit_aliases.h"
#include "vm_jit_record_reads.h"
#include "bytecode_internal.h"
#include "sljitLir.h"
#include "vm_jit_memory.h"
#include <string.h>

#define CTX SLJIT_S0
#define REGS SLJIT_S1
#define BINDS SLJIT_S2
#define FUEL SLJIT_S3
#define EPOCH SLJIT_S4

typedef struct Emitter {
    struct sljit_compiler *c;
    const MarimayBytecodeFunction *function;
    const MarimayBytecodeJitFunction *compiled;
    size_t pc;
    size_t restore;
    bool meter_fuel;
    MarimayBytecodeJitAliases *aliases;
    MarimayBytecodeJitRecordReads *reads;
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
    mmay_bytecode_jit_record_read_write(e->reads, reg);
    const MarimayBytecodeType *types = e->function->register_types.data;
    move(e, types[reg] == MMAY_BYTECODE_TYPE_BOOL ? SLJIT_MOV_U8 : SLJIT_MOV,
        SLJIT_MEM1(REGS), slot(reg), source, 0);
}
static void invalidate(Emitter *e, uint32_t reg) {
    mmay_bytecode_jit_record_read_write(e->reads, reg);
    const MarimayBytecodeType *types = e->function->register_types.data;
    if (!mmay_bytecode_type_is_array(types[reg]) && types[reg] != MMAY_BYTECODE_TYPE_RECORD) return;
    mmay_bytecode_jit_alias_write(e->aliases, reg);
    sljit_sw base = (sljit_sw)(reg * sizeof(MarimayBytecodeJitBinding));
    _Static_assert(offsetof(MarimayManagedArrayView, reference) == 0 && offsetof(MarimayManagedRecordView, reference) == 0, "view identity prefix");
    move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(BINDS), base + (sljit_sw)offsetof(MarimayBytecodeJitBinding, view));
    struct sljit_jump *different = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_MEM1(REGS), slot(reg));
    move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(BINDS), base + (sljit_sw)offsetof(MarimayBytecodeJitBinding, view) + 8);
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
    mmay_bytecode_jit_alias_copy(e->aliases, dst, src);
}
static void leave(Emitter *e) {
    if (e->meter_fuel && e->restore) sljit_emit_op2(e->c, SLJIT_ADD, FUEL, 0, FUEL, 0, SLJIT_IMM, (sljit_sw)e->restore);
    if (e->meter_fuel) move(e, SLJIT_MOV, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, budget), FUEL, 0);
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
static sljit_sw push_address(void) {
    bool (*function)(MarimayBytecodeJitContext *, uint32_t) = mmay_bytecode_jit_array_push;
    sljit_sw address = 0;
    memcpy(&address, &function, sizeof(address));
    return address;
}
static sljit_sw enum_new_address(void) {
    bool (*function)(MarimayBytecodeJitContext *, uint32_t) = mmay_bytecode_jit_enum_new;
    sljit_sw address = 0;
    memcpy(&address, &function, sizeof(address));
    return address;
}
static void reload(Emitter *e) {
    move(e, SLJIT_MOV, REGS, 0, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, registers));
    move(e, SLJIT_MOV, BINDS, 0, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, bindings));
    move(e, SLJIT_MOV, FUEL, 0, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, budget));
    move(e, SLJIT_MOV, EPOCH, 0, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, epoch));
}
static void transition(Emitter *e, const MarimayBytecodeImageOperation *operation) {
    mmay_bytecode_jit_aliases_reset(e->aliases);
    mmay_bytecode_jit_record_reads_reset(e->reads);
    bool is_call = operation->opcode == MMAY_OPCODE_CALL_DIRECT
        || operation->opcode == MMAY_OPCODE_CALL_DIRECT_VOID;
    sljit_sw address = 0;
    const MarimayBytecodeBlock *block = mmay_vector_at_const(
        &e->function->blocks, operation->source_block);
    const MarimayBytecodeInstruction *instruction = block == NULL ? NULL
        : mmay_vector_at_const(&block->instructions, operation->source_instruction);
    if (instruction == NULL) { leave(e); return; }
    move(e, SLJIT_MOV, SLJIT_R0, 0, CTX, 0);
    move(e, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)e->pc);
    move(e, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)(uintptr_t)instruction);
    if (is_call) {
        void *(*function)(MarimayBytecodeJitContext *, uint32_t,
            const MarimayBytecodeInstruction *, const MarimayBytecodePreparedCall *)
            = mmay_bytecode_jit_call;
        _Static_assert(sizeof(address) == sizeof(function), "call pointer");
        memcpy(&address, &function, sizeof(address));
        uint32_t wire = operation->opcode == MMAY_OPCODE_CALL_DIRECT_VOID
            ? operation->first : operation->second;
        move(e, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM,
            (sljit_sw)(uintptr_t)&e->compiled->calls[wire]);
        sljit_emit_icall(e->c, SLJIT_CALL, SLJIT_ARGS4(P, P, 32, P, P), SLJIT_IMM, address);
    } else {
        void *(*function)(MarimayBytecodeJitContext *, uint32_t,
            const MarimayBytecodeInstruction *) = mmay_bytecode_jit_return;
        _Static_assert(sizeof(address) == sizeof(function), "return pointer");
        memcpy(&address, &function, sizeof(address));
        sljit_emit_icall(e->c, SLJIT_CALL, SLJIT_ARGS3(P, P, 32, P), SLJIT_IMM, address);
    }
    struct sljit_jump *resume = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    /* The helper has already published the current logical activation/PC. */
    sljit_emit_return_void(e->c);
    sljit_set_label(resume, sljit_emit_label(e->c));
    reload(e);
    sljit_emit_ijump(e->c, SLJIT_JUMP, SLJIT_R0, 0);
}
static void composed_shift(Emitter *e) {
    bool (*function)(MarimayBytecodeJitContext *, uint32_t) = mmay_bytecode_jit_shift;
    sljit_sw address = 0;
    _Static_assert(sizeof(address) == sizeof(function), "helper pointer");
    memcpy(&address, &function, sizeof(address));
    if (e->meter_fuel) move(e, SLJIT_MOV, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, budget), FUEL, 0);
    move(e, SLJIT_MOV, SLJIT_R0, 0, CTX, 0);
    move(e, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)e->pc);
    sljit_emit_icall(e->c, SLJIT_CALL, SLJIT_ARGS2(32, P, 32), SLJIT_IMM, address);
    struct sljit_jump *unchanged = sljit_emit_cmp(e->c, SLJIT_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 0);
    sljit_emit_return_void(e->c);
    sljit_set_label(unchanged, sljit_emit_label(e->c));
}
/* Inline the retained-view predicate before the unchanged checked miss owner.
 * No object pointer is dereferenced here. Function-specific tables still check
 * every identity component, so recursion and cold GC cannot revive old views. */
static struct sljit_jump *retained_view(Emitter *e, uint32_t reg, sljit_sw base) {
    struct sljit_jump *miss[14];
    size_t n = 0;
    sljit_sw heap = (sljit_sw)offsetof(MarimayBytecodeExecution, managed_heap);
    sljit_sw value = (sljit_sw)(reg * sizeof(MarimayBytecodeValue));
    const uint32_t *layouts = e->function->register_layouts.data;
    _Static_assert(sizeof(MarimayBytecodeType) == 4, "runtime value type word");
    move(e, SLJIT_MOV, SLJIT_R3, 0, SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, execution));
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_EQUAL, SLJIT_R3, 0, SLJIT_IMM, 0);
    move(e, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R3), offsetof(MarimayBytecodeExecution, initialized));
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    move(e, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R3), heap + (sljit_sw)offsetof(MarimayManagedHeap, initialized));
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    move(e, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R3), heap + (sljit_sw)offsetof(MarimayManagedHeap, collecting));
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    move(e, SLJIT_MOV, SLJIT_R4, 0, SLJIT_MEM1(SLJIT_R3), heap + (sljit_sw)offsetof(MarimayManagedHeap, view_epoch));
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_EQUAL, SLJIT_R4, 0, SLJIT_IMM, -1);
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL, SLJIT_R4, 0,
        SLJIT_MEM1(BINDS), base + (sljit_sw)offsetof(MarimayBytecodeJitBinding, view_epoch));
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL, SLJIT_R3, 0,
        SLJIT_MEM1(BINDS), base + (sljit_sw)offsetof(MarimayBytecodeJitBinding, execution));
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL,
        SLJIT_MEM1(BINDS), base + (sljit_sw)offsetof(MarimayBytecodeJitBinding, function),
        SLJIT_IMM, (sljit_sw)(uintptr_t)e->function);
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL,
        SLJIT_MEM1(CTX), offsetof(MarimayBytecodeJitContext, function),
        SLJIT_IMM, (sljit_sw)(uintptr_t)e->function);
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL | SLJIT_32,
        SLJIT_MEM1(BINDS), base + (sljit_sw)offsetof(MarimayBytecodeJitBinding, layout),
        SLJIT_IMM, (sljit_s32)layouts[reg]);
    move(e, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_MEM1(REGS), value + (sljit_sw)offsetof(MarimayBytecodeValue, type));
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_R0, 0,
        SLJIT_MEM1(BINDS), base + (sljit_sw)offsetof(MarimayBytecodeJitBinding, type));
    move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(REGS), slot(reg));
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL, SLJIT_R0, 0,
        SLJIT_MEM1(SLJIT_R3), heap + (sljit_sw)offsetof(MarimayManagedHeap, provenance));
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL, SLJIT_R0, 0,
        SLJIT_MEM1(BINDS), base + (sljit_sw)offsetof(MarimayBytecodeJitBinding, view));
    move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(REGS), slot(reg) + 8);
    miss[n++] = sljit_emit_cmp(e->c, SLJIT_NOT_EQUAL, SLJIT_R0, 0,
        SLJIT_MEM1(BINDS), base + (sljit_sw)offsetof(MarimayBytecodeJitBinding, view) + 8);
    move(e, SLJIT_MOV, SLJIT_MEM1(BINDS), base, EPOCH, 0);
    struct sljit_jump *reused = sljit_emit_jump(e->c, SLJIT_JUMP);
    struct sljit_label *slow = sljit_emit_label(e->c);
    for (size_t i = 0; i < n; ++i) sljit_set_label(miss[i], slow);
    return reused;
}

/* Returns view in R5. No borrowed data address survives a VM safepoint. */
static void bind(Emitter *e, uint32_t reg) {
    reg = mmay_bytecode_jit_alias_source(e->aliases, reg);
    sljit_sw base = (sljit_sw)(reg * sizeof(MarimayBytecodeJitBinding));
    struct sljit_jump *hit = sljit_emit_cmp(e->c, SLJIT_EQUAL,
        SLJIT_MEM1(BINDS), base, EPOCH, 0);
    struct sljit_jump *reused = retained_view(e, reg, base);
    move(e, SLJIT_MOV, SLJIT_R0, 0, CTX, 0);
    move(e, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reg);
    sljit_emit_icall(e->c, SLJIT_CALL, SLJIT_ARGS2(P, P, 32), SLJIT_IMM, bind_address());
    condition(e, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    struct sljit_label *bound = sljit_emit_label(e->c);
    sljit_set_label(hit, bound);
    sljit_set_label(reused, bound);
    sljit_emit_op2(e->c, SLJIT_ADD, SLJIT_R5, 0, BINDS, 0, SLJIT_IMM,
        base + (sljit_sw)offsetof(MarimayBytecodeJitBinding, view));
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
        /* Every published array length is <= INT64_MAX. Unsigned index <
         * length therefore rejects all negative i64 indices as well. */
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
    if (!store && width == sizeof(MarimayManagedCell)) {
        _Static_assert(sizeof(MarimayManagedCell) == 16
            && sizeof(MarimayManagedReference) == 16, "complete reference cell");
        /* SLJIT indexed memory scales only through 8 bytes. Form the 16-byte
         * cell address explicitly, then load both identity words before publish. */
        sljit_emit_op2(e->c, SLJIT_SHL, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 4);
        sljit_emit_op2(e->c, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_R1, 0);
        move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R2), 0);
        move(e, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R2), 8);
        move(e, SLJIT_MOV, SLJIT_MEM1(REGS), slot(op->first), SLJIT_R0, 0);
        move(e, SLJIT_MOV, SLJIT_MEM1(REGS), slot(op->first) + 8, SLJIT_R1, 0);
        invalidate(e, op->first);
        return;
    }
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
    const MarimayBytecodeBlock *block = mmay_vector_at_const(
        &e->function->blocks, op->source_block);
    const MarimayBytecodeInstruction *original = block == NULL ? NULL
        : mmay_vector_at_const(&block->instructions, op->source_instruction);
    /* The compact image's third slot does not retain every signed/wide
     * operand. Project this verified field from its authoritative instruction. */
    if (original == NULL || original->opcode != MMAY_OPCODE_RECORD_GET
        || original->operands[2].signed_value < 0
        || (uint64_t)original->operands[2].signed_value
            > ((uint64_t)INTPTR_MAX - sizeof(MarimayManagedCell)) / sizeof(MarimayManagedCell)) {
        leave(e);
        return;
    }
    uint32_t receiver = mmay_bytecode_jit_alias_source(e->aliases, op->second);
    uint64_t field = (uint64_t)original->operands[2].signed_value;
    uint32_t previous = mmay_bytecode_jit_record_read_find(e->reads, receiver, field, op->first);
    if (previous != UINT32_MAX) {
        copy(e, op->first, previous);
        mmay_bytecode_jit_record_read_remember(e->reads, receiver, field, op->first);
        return;
    }
    sljit_sw field_offset = (sljit_sw)((uint64_t)original->operands[2].signed_value
        * sizeof(MarimayManagedCell));
    bind(e, op->second);
    move(e, SLJIT_MOV, SLJIT_R5, 0, SLJIT_MEM1(SLJIT_R5), offsetof(MarimayManagedRecordView, fields));
    const MarimayBytecodeType *types = e->function->register_types.data;
    bool managed = mmay_bytecode_type_is_managed(types[op->first]);
    size_t count = managed ? sizeof(MarimayManagedReference) / 8 : 1;
    for (size_t part = 0; part < count; ++part) {
        move(e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R5),
            field_offset + (sljit_sw)(part * 8));
        if (!managed && types[op->first] == MMAY_BYTECODE_TYPE_BOOL)
            boolean(e, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        move(e, types[op->first] == MMAY_BYTECODE_TYPE_BOOL ? SLJIT_MOV_U8 : SLJIT_MOV,
            SLJIT_MEM1(REGS), slot(op->first) + (sljit_sw)(part * 8), SLJIT_R0, 0);
    }
    invalidate(e, op->first);
    mmay_bytecode_jit_record_read_remember(e->reads, receiver, field, op->first);
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
    MarimayAllocator *allocator, size_t capacity, MarimayCodeMemoryFault *fault, bool meter_fuel) {
    size_t n = image->operation_count;
    size_t scratch_bytes = n * (sizeof(struct sljit_label *) + 2*sizeof(struct sljit_jump *));
    void *scratch = mmay_allocate(allocator, scratch_bytes);
    if (!scratch) return MMAY_CODE_MEMORY_ALLOCATION_FAILED;
    memset(scratch, 0, scratch_bytes);
    struct sljit_label **labels = scratch;
    struct sljit_jump **edges = (struct sljit_jump **)(labels + n);
    struct sljit_compiler *compiler = sljit_create_compiler(allocator);
    if (!compiler) { mmay_deallocate(allocator, scratch, scratch_bytes); return MMAY_CODE_MEMORY_ALLOCATION_FAILED; }
    uint32_t alias_storage[MMAY_JIT_MAX_REGISTERS];
    MarimayBytecodeJitAliases aliases;
    mmay_bytecode_jit_aliases_init(&aliases, function, alias_storage);
    MarimayBytecodeJitRecordRead read_storage[MMAY_JIT_MAX_REGISTERS];
    MarimayBytecodeJitRecordReads reads;
    mmay_bytecode_jit_record_reads_init(&reads, function, read_storage);
    Emitter e = {compiler, function, compiled, 0, 0, meter_fuel, &aliases, &reads};
    sljit_emit_enter(compiler, 0, SLJIT_ARGS2V(P, P), 6, 5, 0);
    move(&e, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S1, 0);
    reload(&e);
    sljit_emit_ijump(compiler, SLJIT_JUMP, SLJIT_R0, 0);
    for (size_t pc = 0; pc < n; ++pc) {
        const MarimayBytecodeImageOperation *op = &image->operations[pc];
        const MarimayBytecodeJitPlanOperation *plan = &compiled->plan[pc];
        if (plan->starts_segment || !plan->supported) {
            mmay_bytecode_jit_aliases_reset(&aliases);
            mmay_bytecode_jit_record_reads_reset(&reads);
        }
        labels[pc] = sljit_emit_label(compiler);
        e.pc = pc; e.restore = 0;
        if (!plan->supported) { leave(&e); continue; }
        if (plan->starts_segment && op->fusion == MMAY_BYTECODE_IMAGE_FUSION_ARRAY_SHIFT_LOOP)
            composed_shift(&e);
        if (plan->starts_segment && meter_fuel) {
            condition(&e, SLJIT_GREATER_EQUAL, FUEL, 0, SLJIT_IMM, (sljit_sw)(plan->segment_end - pc));
            sljit_emit_op2(compiler, SLJIT_SUB, FUEL, 0, FUEL, 0, SLJIT_IMM, (sljit_sw)(plan->segment_end - pc));
        }
        e.restore = meter_fuel ? plan->segment_end - pc : 0;
        switch (op->opcode) {
            case MMAY_OPCODE_CALL_DIRECT: case MMAY_OPCODE_CALL_DIRECT_VOID:
            case MMAY_OPCODE_RETURN: case MMAY_OPCODE_RETURN_VOID:
                transition(&e, op); break;
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
            case MMAY_OPCODE_ARRAY_PUSH:
            case MMAY_OPCODE_ENUM_NEW:
            case MMAY_OPCODE_ENUM_IS_VARIANT: case MMAY_OPCODE_ENUM_REQUIRE_VARIANT:
            case MMAY_OPCODE_ENUM_GET_PAYLOAD:
                move(&e, SLJIT_MOV, SLJIT_R0, 0, CTX, 0);
                move(&e, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, (sljit_sw)pc);
                sljit_emit_icall(compiler, SLJIT_CALL, SLJIT_ARGS2(32, P, 32), SLJIT_IMM,
                    op->opcode == MMAY_OPCODE_ARRAY_PUSH ? push_address()
                        : op->opcode == MMAY_OPCODE_ENUM_NEW ? enum_new_address() : enum_address());
                condition(&e, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 0);
                /* A successful push changes the object, never its reference. */
                if (op->opcode != MMAY_OPCODE_ARRAY_PUSH) invalidate(&e, op->first);
                break;
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
