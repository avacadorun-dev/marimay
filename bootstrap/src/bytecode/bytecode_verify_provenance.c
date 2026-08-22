#include "bytecode_verify_provenance.h"

#include <limits.h>
#include <string.h>

#include "bytecode_internal.h"

#define RUDA_BYTECODE_PROVENANCE_TOP UINT64_MAX
#define RUDA_BYTECODE_PROVENANCE_NONE (UINT64_MAX - 1U)

typedef enum RudaBytecodeProvenanceKind {
    RUDA_BYTECODE_PROVENANCE_ARRAY_INDEX,
    RUDA_BYTECODE_PROVENANCE_ENUM_VARIANT
} RudaBytecodeProvenanceKind;

struct RudaBytecodeProvenanceAnalysis {
    RudaAllocator allocator;
    size_t register_count;
    size_t block_count;
    size_t cell_count;
    size_t cells_size;
    size_t scratch_size;
    uint64_t *array_incoming;
    uint64_t *array_scratch;
    uint64_t *enum_incoming;
    uint64_t *enum_scratch;
};

static bool ruda_bytecode_provenance_consume_work(
    size_t *used,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (used == NULL || !ruda_size_add(*used, amount, &next) || next > limit) {
        return false;
    }
    *used = next;
    return true;
}

static uint64_t ruda_bytecode_provenance_meet(
    uint64_t left,
    uint64_t right
) {
    if (left == RUDA_BYTECODE_PROVENANCE_TOP) {
        return right;
    }
    if (right == RUDA_BYTECODE_PROVENANCE_TOP || left == right) {
        return left;
    }
    return RUDA_BYTECODE_PROVENANCE_NONE;
}

static void ruda_bytecode_provenance_transfer_instruction(
    const RudaBytecodeInstruction *instruction,
    uint64_t *facts,
    size_t register_count,
    RudaBytecodeProvenanceKind kind
) {
    uint64_t target = 0;
    size_t register_index = 0;
    if (instruction == NULL || facts == NULL) {
        return;
    }
    if (kind == RUDA_BYTECODE_PROVENANCE_ARRAY_INDEX
        && (instruction->opcode == RUDA_OPCODE_CALL_DIRECT
            || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID
            || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER
            || instruction->opcode == RUDA_OPCODE_CALL_INTERFACE
            || instruction->opcode == RUDA_OPCODE_ARRAY_SET
            || instruction->opcode == RUDA_OPCODE_ARRAY_SET_IN_BOUNDS
            || instruction->opcode == RUDA_OPCODE_ARRAY_PUSH
            || instruction->opcode == RUDA_OPCODE_ARRAY_POP
            || instruction->opcode == RUDA_OPCODE_ARRAY_TRUNCATE
            || instruction->opcode == RUDA_OPCODE_ARRAY_CLEAR
            || instruction->opcode == RUDA_OPCODE_ARRAY_RESERVE)) {
        for (register_index = 0; register_index < register_count;
            register_index += 1) {
            facts[register_index] = RUDA_BYTECODE_PROVENANCE_NONE;
        }
        return;
    }
    if (!ruda_bytecode_opcode_writes_register(instruction->opcode)) {
        return;
    }
    target = instruction->operands[0].unsigned_value;
    if (target >= register_count) {
        return;
    }
    if (kind == RUDA_BYTECODE_PROVENANCE_ARRAY_INDEX) {
        for (register_index = 0; register_index < register_count;
            register_index += 1) {
            if (facts[register_index] == target) {
                facts[register_index] = RUDA_BYTECODE_PROVENANCE_NONE;
            }
        }
        facts[(size_t)target] = instruction->opcode
                == RUDA_OPCODE_ARRAY_REQUIRE_INDEX
            ? instruction->operands[2].unsigned_value
            : RUDA_BYTECODE_PROVENANCE_NONE;
        return;
    }
    facts[(size_t)target] = instruction->opcode
            == RUDA_OPCODE_ENUM_REQUIRE_VARIANT
        ? (uint64_t)instruction->operands[2].signed_value
        : RUDA_BYTECODE_PROVENANCE_NONE;
}

static void ruda_bytecode_provenance_transfer_block(
    const RudaBytecodeBlock *block,
    uint64_t *facts,
    size_t register_count,
    RudaBytecodeProvenanceKind kind
) {
    size_t instruction_index = 0;
    if (block == NULL || facts == NULL) {
        return;
    }
    for (instruction_index = 0;
        instruction_index + 1 < block->instructions.length;
        instruction_index += 1) {
        const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
            &block->instructions,
            instruction_index
        );
        ruda_bytecode_provenance_transfer_instruction(
            instruction,
            facts,
            register_count,
            kind
        );
    }
}

static RudaBytecodeVerifyStatus ruda_bytecode_provenance_solve(
    const RudaBytecodeFunction *function,
    const size_t *predecessor_offsets,
    const uint32_t *predecessors,
    uint64_t *incoming,
    uint64_t *outgoing,
    uint64_t *scratch,
    size_t scratch_size,
    RudaBytecodeProvenanceKind kind,
    size_t available_work,
    size_t *work_used
) {
    size_t register_count = function->register_types.length;
    size_t block_count = function->blocks.length;
    size_t cell_count = 0;
    size_t register_index = 0;
    size_t entry_work = 0;
    size_t work = 0;
    bool changed = false;
    const RudaBytecodeBlock *entry = ruda_vector_at_const(
        &function->blocks,
        0
    );
    if (!ruda_size_multiply(block_count, register_count, &cell_count)
        || !ruda_size_multiply(
            entry->instructions.length,
            register_count,
            &entry_work
        )) {
        return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    for (register_index = 0; register_index < cell_count;
        register_index += 1) {
        incoming[register_index] = RUDA_BYTECODE_PROVENANCE_TOP;
        outgoing[register_index] = RUDA_BYTECODE_PROVENANCE_TOP;
    }
    for (register_index = 0; register_index < register_count;
        register_index += 1) {
        incoming[register_index] = RUDA_BYTECODE_PROVENANCE_NONE;
        outgoing[register_index] = RUDA_BYTECODE_PROVENANCE_NONE;
    }
    if (!ruda_bytecode_provenance_consume_work(
            &work,
            entry_work,
            available_work
        )) {
        return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    ruda_bytecode_provenance_transfer_block(
        entry,
        outgoing,
        register_count,
        kind
    );
    do {
        size_t block_index = 0;
        changed = false;
        for (block_index = 1; block_index < block_count; block_index += 1) {
            size_t predecessor_index = 0;
            uint64_t *incoming_row = incoming
                + block_index * register_count;
            uint64_t *outgoing_row = outgoing
                + block_index * register_count;
            const RudaBytecodeBlock *block = ruda_vector_at_const(
                &function->blocks,
                block_index
            );
            size_t transfer_work = 0;
            for (register_index = 0; register_index < register_count;
                register_index += 1) {
                scratch[register_index] = RUDA_BYTECODE_PROVENANCE_TOP;
            }
            for (predecessor_index = predecessor_offsets[block_index];
                predecessor_index < predecessor_offsets[block_index + 1];
                predecessor_index += 1) {
                const uint64_t *predecessor_row = outgoing
                    + (size_t)predecessors[predecessor_index]
                        * register_count;
                for (register_index = 0; register_index < register_count;
                    register_index += 1) {
                    scratch[register_index] = ruda_bytecode_provenance_meet(
                        scratch[register_index],
                        predecessor_row[register_index]
                    );
                }
                if (!ruda_bytecode_provenance_consume_work(
                        &work,
                        register_count,
                        available_work
                    )) {
                    return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
                }
            }
            if (register_count != 0
                && memcmp(incoming_row, scratch, scratch_size) != 0) {
                memcpy(incoming_row, scratch, scratch_size);
                changed = true;
            }
            if (!ruda_size_multiply(
                    block->instructions.length,
                    register_count,
                    &transfer_work
                ) || !ruda_bytecode_provenance_consume_work(
                    &work,
                    transfer_work,
                    available_work
                )) {
                return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
            }
            ruda_bytecode_provenance_transfer_block(
                block,
                scratch,
                register_count,
                kind
            );
            if (register_count != 0
                && memcmp(outgoing_row, scratch, scratch_size) != 0) {
                memcpy(outgoing_row, scratch, scratch_size);
                changed = true;
            }
        }
    } while (changed);
    *work_used = work;
    return RUDA_BYTECODE_VERIFY_OK;
}

static RudaBytecodeVerifyStatus ruda_bytecode_provenance_add_lattice(
    RudaBytecodeProvenanceAnalysis *analysis,
    const RudaBytecodeFunction *function,
    const size_t *predecessor_offsets,
    const uint32_t *predecessors,
    RudaBytecodeProvenanceKind kind,
    size_t available_work,
    size_t *work_used
) {
    uint64_t **incoming = kind == RUDA_BYTECODE_PROVENANCE_ARRAY_INDEX
        ? &analysis->array_incoming
        : &analysis->enum_incoming;
    uint64_t **scratch = kind == RUDA_BYTECODE_PROVENANCE_ARRAY_INDEX
        ? &analysis->array_scratch
        : &analysis->enum_scratch;
    uint64_t *outgoing = NULL;
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    *incoming = ruda_allocate_array(
        &analysis->allocator,
        analysis->cell_count,
        sizeof(uint64_t)
    );
    outgoing = ruda_allocate_array(
        &analysis->allocator,
        analysis->cell_count,
        sizeof(uint64_t)
    );
    *scratch = ruda_allocate_array(
        &analysis->allocator,
        analysis->register_count,
        sizeof(uint64_t)
    );
    if (*incoming == NULL || outgoing == NULL || *scratch == NULL) {
        status = RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED;
    } else {
        status = ruda_bytecode_provenance_solve(
            function,
            predecessor_offsets,
            predecessors,
            *incoming,
            outgoing,
            *scratch,
            analysis->scratch_size,
            kind,
            available_work,
            work_used
        );
    }
    ruda_deallocate(
        &analysis->allocator,
        outgoing,
        analysis->cells_size
    );
    return status;
}

RudaBytecodeVerifyStatus ruda_bytecode_provenance_analyze(
    const RudaBytecodeFunction *function,
    const size_t *predecessor_offsets,
    const uint32_t *predecessors,
    bool check_array_indices,
    bool check_enum_variants,
    size_t available_work,
    size_t *work_used,
    RudaBytecodeProvenanceAnalysis **result
) {
    RudaBytecodeProvenanceAnalysis *analysis = NULL;
    size_t cell_count = 0;
    size_t cells_size = 0;
    size_t lattice_work = 0;
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    if (work_used == NULL || result == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    *work_used = 0;
    *result = NULL;
    if (!check_array_indices && !check_enum_variants) {
        return RUDA_BYTECODE_VERIFY_OK;
    }
    if (function == NULL || predecessor_offsets == NULL
        || predecessors == NULL || function->register_types.length == 0
        || !ruda_size_multiply(
            function->blocks.length,
            function->register_types.length,
            &cell_count
        ) || !ruda_size_multiply(
            cell_count,
            sizeof(uint64_t),
            &cells_size
        )) {
        return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    analysis = ruda_allocate(&function->allocator, sizeof(*analysis));
    if (analysis == NULL) {
        return RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED;
    }
    memset(analysis, 0, sizeof(*analysis));
    analysis->allocator = function->allocator;
    analysis->register_count = function->register_types.length;
    analysis->block_count = function->blocks.length;
    analysis->cell_count = cell_count;
    analysis->cells_size = cells_size;
    if (!ruda_size_multiply(
            analysis->register_count,
            sizeof(uint64_t),
            &analysis->scratch_size
        )) {
        status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        goto fail;
    }
    if (check_array_indices) {
        status = ruda_bytecode_provenance_add_lattice(
            analysis,
            function,
            predecessor_offsets,
            predecessors,
            RUDA_BYTECODE_PROVENANCE_ARRAY_INDEX,
            available_work,
            &lattice_work
        );
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            goto fail;
        }
        *work_used = lattice_work;
    }
    if (check_enum_variants) {
        status = ruda_bytecode_provenance_add_lattice(
            analysis,
            function,
            predecessor_offsets,
            predecessors,
            RUDA_BYTECODE_PROVENANCE_ENUM_VARIANT,
            available_work - *work_used,
            &lattice_work
        );
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            goto fail;
        }
        *work_used += lattice_work;
    }
    *result = analysis;
    return RUDA_BYTECODE_VERIFY_OK;

fail:
    ruda_bytecode_provenance_dispose(analysis);
    return status;
}

void ruda_bytecode_provenance_begin_block(
    RudaBytecodeProvenanceAnalysis *analysis,
    size_t block_index
) {
    if (analysis == NULL || block_index >= analysis->block_count) {
        return;
    }
    if (analysis->array_incoming != NULL) {
        memcpy(
            analysis->array_scratch,
            analysis->array_incoming
                + block_index * analysis->register_count,
            analysis->scratch_size
        );
    }
    if (analysis->enum_incoming != NULL) {
        memcpy(
            analysis->enum_scratch,
            analysis->enum_incoming
                + block_index * analysis->register_count,
            analysis->scratch_size
        );
    }
}

RudaBytecodeVerifyStatus ruda_bytecode_provenance_validate_instruction(
    const RudaBytecodeProvenanceAnalysis *analysis,
    const RudaBytecodeInstruction *instruction
) {
    uint64_t array_register = 0;
    uint64_t index_register = 0;
    if (analysis == NULL || instruction == NULL) {
        return RUDA_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == RUDA_OPCODE_ARRAY_GET_IN_BOUNDS) {
        array_register = instruction->operands[1].unsigned_value;
        index_register = instruction->operands[2].unsigned_value;
    } else {
        array_register = instruction->operands[0].unsigned_value;
        index_register = instruction->operands[1].unsigned_value;
    }
    if ((instruction->opcode == RUDA_OPCODE_ARRAY_SET
            || instruction->opcode == RUDA_OPCODE_ARRAY_GET_IN_BOUNDS
            || instruction->opcode == RUDA_OPCODE_ARRAY_SET_IN_BOUNDS)
        && (analysis->array_scratch == NULL
            || analysis->array_scratch[
                (size_t)array_register
            ] != index_register)) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    if (instruction->opcode == RUDA_OPCODE_ENUM_GET_PAYLOAD) {
        uint64_t selector = (uint64_t)instruction->operands[2].signed_value;
        uint64_t enum_register = instruction->operands[1].unsigned_value;
        if (analysis->enum_scratch == NULL
            || analysis->enum_scratch[(size_t)enum_register]
                != selector >> 32) {
            return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
    }
    return RUDA_BYTECODE_VERIFY_OK;
}

RudaBytecodeVerifyStatus ruda_bytecode_provenance_advance_instruction(
    RudaBytecodeProvenanceAnalysis *analysis,
    const RudaBytecodeInstruction *instruction,
    size_t available_work,
    size_t *work_used
) {
    size_t transfer_work = 0;
    if (work_used == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    *work_used = 0;
    if (analysis == NULL || instruction == NULL
        || !ruda_bytecode_opcode_writes_register(instruction->opcode)) {
        return RUDA_BYTECODE_VERIFY_OK;
    }
    if (!ruda_size_multiply(
            analysis->register_count,
            (analysis->array_scratch != NULL ? 1U : 0U)
                + (analysis->enum_scratch != NULL ? 1U : 0U),
            &transfer_work
        ) || transfer_work > available_work) {
        return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    if (analysis->array_scratch != NULL) {
        ruda_bytecode_provenance_transfer_instruction(
            instruction,
            analysis->array_scratch,
            analysis->register_count,
            RUDA_BYTECODE_PROVENANCE_ARRAY_INDEX
        );
    }
    if (analysis->enum_scratch != NULL) {
        ruda_bytecode_provenance_transfer_instruction(
            instruction,
            analysis->enum_scratch,
            analysis->register_count,
            RUDA_BYTECODE_PROVENANCE_ENUM_VARIANT
        );
    }
    *work_used = transfer_work;
    return RUDA_BYTECODE_VERIFY_OK;
}

void ruda_bytecode_provenance_dispose(
    RudaBytecodeProvenanceAnalysis *analysis
) {
    RudaAllocator allocator;
    if (analysis == NULL) {
        return;
    }
    allocator = analysis->allocator;
    ruda_deallocate(
        &allocator,
        analysis->enum_scratch,
        analysis->scratch_size
    );
    ruda_deallocate(
        &allocator,
        analysis->enum_incoming,
        analysis->cells_size
    );
    ruda_deallocate(
        &allocator,
        analysis->array_scratch,
        analysis->scratch_size
    );
    ruda_deallocate(
        &allocator,
        analysis->array_incoming,
        analysis->cells_size
    );
    ruda_deallocate(&allocator, analysis, sizeof(*analysis));
}
