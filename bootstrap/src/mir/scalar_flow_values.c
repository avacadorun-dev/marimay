#include "scalar_flow_internal.h"
#include "mir_internal.h"
#include "mmay/base/checked_i64_fast.h"

#include <string.h>

typedef struct ScalarFact {
    uint64_t bits;
    MarimayMirRegister alias;
    MarimayMirRegister negated_source;
    bool known;
} ScalarFact;

static bool flow_definitions(MarimayMirScalarFlow *flow, unsigned char *definitions) {
    const MarimayMirFunction *function = flow->function;
    memset(definitions, 0, function->register_types.length);
    for (size_t i = 0; i < function->parameter_count; i += 1) definitions[i] = 1;
    for (size_t b = 0; b < function->blocks.length; b += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(&function->blocks, b);
        for (size_t i = 0; i < block->instructions.length; i += 1) {
            if (!mmay_mir_scalar_flow_work(flow, 1)) return false;
            const MarimayMirInstruction *ins = mmay_vector_at_const(&block->instructions, i);
            if (definitions[ins->target] < 2) definitions[ins->target] += 1;
        }
    }
    return true;
}

MarimayMirScalarFlowStatus mmay_mir_scalar_flow_hoist(MarimayMirScalarFlow *flow) {
    MarimayMirFunction *function = flow->function;
    size_t bytes = function->register_types.length == 0 ? 1 : function->register_types.length;
    unsigned char *definitions = mmay_allocate(&function->allocator, bytes);
    MarimayVector entry = {0};
    MarimayMirScalarFlowStatus status = MMAY_MIR_SCALAR_FLOW_ALLOCATION_FAILED;
    if (definitions == NULL || !mmay_vector_init(&entry, function->allocator,
            sizeof(MarimayMirInstruction))) goto cleanup;
    if (!flow_definitions(flow, definitions)) { status = MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED; goto cleanup; }
    for (size_t b = 0; b < function->blocks.length; b += 1) {
        MarimayMirBlock *block = mmay_vector_at(&function->blocks, b);
        for (size_t i = 0; i < block->instructions.length; i += 1) {
            if (!mmay_mir_scalar_flow_work(flow, 1)) { status = MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED; goto cleanup; }
            const MarimayMirInstruction *ins = mmay_vector_at_const(&block->instructions, i);
            if (definitions[ins->target] == 1 && mmay_mir_scalar_flow_literal(ins->kind)
                && !mmay_vector_push(&entry, ins)) goto cleanup;
        }
    }
    for (size_t b = 0; b < function->blocks.length; b += 1) {
        MarimayMirBlock *block = mmay_vector_at(&function->blocks, b);
        MarimayMirInstruction *instructions = (MarimayMirInstruction *)block->instructions.data;
        size_t kept = 0;
        for (size_t i = 0; i < block->instructions.length; i += 1) {
            if (!mmay_mir_scalar_flow_work(flow, 1)) { status = MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED; goto cleanup; }
            MarimayMirInstruction ins = instructions[i];
            if (definitions[ins.target] == 1 && mmay_mir_scalar_flow_literal(ins.kind)) continue;
            if (b == 0) {
                if (!mmay_vector_push(&entry, &ins)) goto cleanup;
            } else {
                instructions[kept++] = ins;
            }
        }
        if (b != 0) block->instructions.length = kept;
    }
    MarimayMirBlock *first = mmay_vector_at(&function->blocks, 0);
    mmay_vector_dispose(&first->instructions);
    first->instructions = entry;
    memset(&entry, 0, sizeof(entry));
    status = MMAY_MIR_SCALAR_FLOW_OK;
cleanup:
    mmay_vector_dispose(&entry);
    mmay_deallocate(&function->allocator, definitions, bytes);
    return status;
}

static uint64_t flow_literal_bits(const MarimayMirInstruction *instruction) {
    if (instruction->kind == MMAY_MIR_CONSTANT_BOOL) return instruction->boolean ? 1 : 0;
    if (instruction->kind == MMAY_MIR_CONSTANT_I64) return (uint64_t)instruction->integer;
    return instruction->kind == MMAY_MIR_CONSTANT_F64 ? instruction->f64_bits : instruction->integer_bits;
}

static void flow_literal(MarimayMirInstruction *instruction, MarimayTypeId type, uint64_t bits) {
    MarimayMirRegister target = instruction->target;
    MarimayMirSpan span = instruction->span;
    memset(instruction, 0, sizeof(*instruction));
    instruction->target = target;
    instruction->span = span;
    if (type == MMAY_TYPE_BOOL) {
        instruction->kind = MMAY_MIR_CONSTANT_BOOL;
        instruction->boolean = bits != 0;
    } else if (type == MMAY_TYPE_I64) {
        instruction->kind = MMAY_MIR_CONSTANT_I64;
        instruction->integer = mmay_i64_from_twos_complement_fast(bits);
    } else if (type == MMAY_TYPE_F64) {
        instruction->kind = MMAY_MIR_CONSTANT_F64;
        instruction->f64_bits = bits;
    } else {
        instruction->kind = MMAY_MIR_CONSTANT_INTEGER;
        instruction->integer_bits = bits;
    }
}

/* Only scalar computations accept local operand substitutions. In particular,
 * array/index proof carriers and metadata arguments keep their exact ids. */
static bool flow_scalar_operands(const MarimayMirInstruction *instruction) {
    switch (instruction->kind) {
        case MMAY_MIR_COPY:
        case MMAY_MIR_NOT_BOOL:
        case MMAY_MIR_EQUAL_BOOL:
        case MMAY_MIR_EQUAL_I64:
        case MMAY_MIR_LESS_I64:
        case MMAY_MIR_LESS_EQUAL_I64:
        case MMAY_MIR_ADD_CHECKED_I64:
        case MMAY_MIR_SUBTRACT_CHECKED_I64:
        case MMAY_MIR_MULTIPLY_CHECKED_I64:
        case MMAY_MIR_DIVIDE_CHECKED_I64:
        case MMAY_MIR_REMAINDER_CHECKED_I64:
        case MMAY_MIR_NEGATE_CHECKED_I64:
        case MMAY_MIR_ADD_PROVEN_I64:
        case MMAY_MIR_SUBTRACT_PROVEN_I64:
        case MMAY_MIR_MULTIPLY_PROVEN_I64:
        case MMAY_MIR_BITWISE_NOT_I64:
        case MMAY_MIR_BITWISE_AND_I64:
        case MMAY_MIR_BITWISE_OR_I64:
        case MMAY_MIR_BITWISE_XOR_I64:
        case MMAY_MIR_SHIFT_LEFT_CHECKED_I64:
        case MMAY_MIR_SHIFT_RIGHT_I64:
            return true;
        default: return false;
    }
}

static bool flow_fold(const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction, const ScalarFact *facts, uint64_t *bits) {
    if (mmay_mir_scalar_flow_literal(instruction->kind)) {
        *bits = flow_literal_bits(instruction);
        return true;
    }
    if (!flow_scalar_operands(instruction) || !facts[instruction->first].known) return false;
    uint64_t left_bits = facts[instruction->first].bits;
    if (instruction->kind == MMAY_MIR_COPY) { *bits = left_bits; return true; }
    if (instruction->kind == MMAY_MIR_NOT_BOOL) { *bits = left_bits == 0; return true; }
    bool unary = instruction->kind == MMAY_MIR_NEGATE_CHECKED_I64
        || instruction->kind == MMAY_MIR_BITWISE_NOT_I64;
    if (!unary && !facts[instruction->second].known) return false;
    uint64_t right_bits = unary ? 0 : facts[instruction->second].bits;
    if (instruction->kind == MMAY_MIR_EQUAL_BOOL) { *bits = left_bits == right_bits; return true; }
    if (mmay_mir_register_type(function, instruction->first) != MMAY_TYPE_I64) return false;
    int64_t left = mmay_i64_from_twos_complement_fast(left_bits);
    int64_t right = mmay_i64_from_twos_complement_fast(right_bits), value = 0;
    MarimayCheckedI64Status status;
    switch (instruction->kind) {
        case MMAY_MIR_EQUAL_I64: *bits = left == right; return true;
        case MMAY_MIR_LESS_I64: *bits = left < right; return true;
        case MMAY_MIR_LESS_EQUAL_I64: *bits = left <= right; return true;
        case MMAY_MIR_ADD_CHECKED_I64:
        case MMAY_MIR_ADD_PROVEN_I64: status = mmay_checked_i64_add_fast(left, right, &value); break;
        case MMAY_MIR_SUBTRACT_CHECKED_I64:
        case MMAY_MIR_SUBTRACT_PROVEN_I64: status = mmay_checked_i64_subtract_fast(left, right, &value); break;
        case MMAY_MIR_MULTIPLY_CHECKED_I64:
        case MMAY_MIR_MULTIPLY_PROVEN_I64: status = mmay_checked_i64_multiply_fast(left, right, &value); break;
        case MMAY_MIR_DIVIDE_CHECKED_I64: status = mmay_checked_i64_divide_fast(left, right, &value); break;
        case MMAY_MIR_REMAINDER_CHECKED_I64: status = mmay_checked_i64_remainder_fast(left, right, &value); break;
        case MMAY_MIR_NEGATE_CHECKED_I64: status = mmay_checked_i64_negate_fast(left, &value); break;
        default: return false;
    }
    if (status != MMAY_CHECKED_I64_OK) return false;
    *bits = (uint64_t)value;
    return true;
}

static void flow_substitute(MarimayMirScalarFlow *flow, const ScalarFact *facts,
    MarimayMirRegister *reg) {
    if (mmay_mir_scalar_flow_scalar(mmay_mir_register_type(flow->function, *reg))
        && *reg != facts[*reg].alias) {
        *reg = facts[*reg].alias;
        flow->changed = true;
    }
}

MarimayMirScalarFlowStatus mmay_mir_scalar_flow_values(MarimayMirScalarFlow *flow) {
    MarimayMirFunction *function = flow->function;
    size_t count = function->register_types.length;
    size_t def_bytes = count == 0 ? 1 : count, fact_bytes = 0;
    if (!mmay_size_multiply(def_bytes, sizeof(ScalarFact), &fact_bytes)) return MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED;
    unsigned char *definitions = mmay_allocate(&function->allocator, def_bytes);
    ScalarFact *initial = mmay_allocate(&function->allocator, fact_bytes);
    ScalarFact *facts = mmay_allocate(&function->allocator, fact_bytes);
    MarimayMirScalarFlowStatus status = MMAY_MIR_SCALAR_FLOW_ALLOCATION_FAILED;
    if (definitions == NULL || initial == NULL || facts == NULL) goto cleanup;
    status = MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED;
    if (!flow_definitions(flow, definitions)) goto cleanup;
    memset(initial, 0, fact_bytes);
    for (size_t i = 0; i < count; i += 1) {
        initial[i].alias = (MarimayMirRegister)i;
        initial[i].negated_source = MMAY_MIR_NO_REGISTER;
    }
    for (size_t b = 0; b < function->blocks.length; b += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(&function->blocks, b);
        for (size_t i = 0; i < block->instructions.length; i += 1) {
            if (!mmay_mir_scalar_flow_work(flow, 1)) goto cleanup;
            const MarimayMirInstruction *ins = mmay_vector_at_const(&block->instructions, i);
            if (definitions[ins->target] == 1 && mmay_mir_scalar_flow_literal(ins->kind)) {
                initial[ins->target].known = true;
                initial[ins->target].bits = flow_literal_bits(ins);
            }
        }
    }
    for (size_t b = 0; b < function->blocks.length; b += 1) {
        if (!mmay_mir_scalar_flow_work(flow, count)) goto cleanup;
        memcpy(facts, initial, fact_bytes);
        MarimayMirBlock *block = mmay_vector_at(&function->blocks, b);
        for (size_t i = 0; i < block->instructions.length; i += 1) {
            if (!mmay_mir_scalar_flow_work(flow, count)) goto cleanup;
            MarimayMirInstruction *ins = mmay_vector_at(&block->instructions, i);
            bool scalar = mmay_mir_scalar_flow_scalar(mmay_mir_register_type(function, ins->target));
            if (scalar && flow_scalar_operands(ins)) {
                flow_substitute(flow, facts, &ins->first);
                if (mmay_mir_instruction_uses_second(ins->kind)) flow_substitute(flow, facts, &ins->second);
            }
            uint64_t bits = 0;
            bool known = scalar && flow_fold(function, ins, facts, &bits);
            MarimayMirRegister alias = ins->target, negated = MMAY_MIR_NO_REGISTER;
            if (scalar && ins->kind == MMAY_MIR_COPY && ins->target != ins->first) {
                alias = ins->first;
                negated = facts[ins->first].negated_source;
            } else if (ins->kind == MMAY_MIR_NOT_BOOL && ins->target != ins->first) {
                negated = ins->first;
            }
            for (size_t r = 0; r < count; r += 1) {
                if (facts[r].alias == ins->target) facts[r].alias = (MarimayMirRegister)r;
                if (facts[r].negated_source == ins->target) facts[r].negated_source = MMAY_MIR_NO_REGISTER;
            }
            facts[ins->target].alias = alias;
            facts[ins->target].negated_source = negated == ins->target ? MMAY_MIR_NO_REGISTER : negated;
            facts[ins->target].known = known;
            facts[ins->target].bits = bits;
            if (known && !mmay_mir_scalar_flow_literal(ins->kind)) {
                flow_literal(ins, mmay_mir_register_type(function, ins->target), bits);
                flow->changed = true;
            }
        }
        MarimayMirTerminator *term = &block->terminator;
        if (term->kind == MMAY_MIR_GOTO) {
            const MarimayMirBlock *next = mmay_vector_at_const(&function->blocks, term->target);
            if (next->instructions.length == 0 && next->terminator.kind == MMAY_MIR_BRANCH) {
                *term = next->terminator;
                flow->changed = true;
            }
        }
        if (term->kind == MMAY_MIR_BRANCH) {
            flow_substitute(flow, facts, &term->value);
            if (facts[term->value].known) {
                if (facts[term->value].bits == 0) term->target = term->otherwise;
                term->kind = MMAY_MIR_GOTO;
                flow->changed = true;
            } else if (facts[term->value].negated_source != MMAY_MIR_NO_REGISTER) {
                term->value = facts[term->value].negated_source;
                MarimayMirBlockId target = term->target;
                term->target = term->otherwise;
                term->otherwise = target;
                flow->changed = true;
            }
        } else if (term->kind == MMAY_MIR_RETURN && term->value != MMAY_MIR_NO_REGISTER) {
            flow_substitute(flow, facts, &term->value);
        }
    }
    status = MMAY_MIR_SCALAR_FLOW_OK;
cleanup:
    mmay_deallocate(&function->allocator, facts, fact_bytes);
    mmay_deallocate(&function->allocator, initial, fact_bytes);
    mmay_deallocate(&function->allocator, definitions, def_bytes);
    return status;
}
