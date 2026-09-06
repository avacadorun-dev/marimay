#include "bytecode_verify_provenance.h"

#include <limits.h>
#include <string.h>

#include "bytecode_internal.h"

#define MMAY_BYTECODE_PROVENANCE_TOP UINT64_MAX
#define MMAY_BYTECODE_PROVENANCE_NONE (UINT64_MAX - 1U)

typedef enum MarimayBytecodeProvenanceKind {
    MMAY_BYTECODE_PROVENANCE_ARRAY_INDEX,
    MMAY_BYTECODE_PROVENANCE_ENUM_VARIANT
} MarimayBytecodeProvenanceKind;

struct MarimayBytecodeProvenanceAnalysis {
    MarimayAllocator allocator;
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

static bool mmay_bytecode_provenance_consume_work(
    size_t *used,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (used == NULL || !mmay_size_add(*used, amount, &next) || next > limit) {
        return false;
    }
    *used = next;
    return true;
}

static uint64_t mmay_bytecode_provenance_meet(
    uint64_t left,
    uint64_t right
) {
    if (left == MMAY_BYTECODE_PROVENANCE_TOP) {
        return right;
    }
    if (right == MMAY_BYTECODE_PROVENANCE_TOP || left == right) {
        return left;
    }
    return MMAY_BYTECODE_PROVENANCE_NONE;
}

static bool mmay_bytecode_provenance_invalidates_arrays(MarimayOpcode opcode) {
    return opcode == MMAY_OPCODE_CALL_DIRECT
        || opcode == MMAY_OPCODE_CALL_DIRECT_VOID
        || opcode == MMAY_OPCODE_CALL_DIRECT_NEVER
        || opcode == MMAY_OPCODE_CALL_INTERFACE
        || opcode == MMAY_OPCODE_ARRAY_SET
        || opcode == MMAY_OPCODE_ARRAY_SET_IN_BOUNDS
        || opcode == MMAY_OPCODE_ARRAY_PUSH
        || opcode == MMAY_OPCODE_ARRAY_POP
        || opcode == MMAY_OPCODE_ARRAY_TRUNCATE
        || opcode == MMAY_OPCODE_ARRAY_CLEAR
        || opcode == MMAY_OPCODE_ARRAY_RESERVE;
}

static void mmay_bytecode_provenance_transfer_instruction(
    const MarimayBytecodeInstruction *instruction,
    uint64_t *facts,
    size_t register_count,
    MarimayBytecodeProvenanceKind kind
) {
    uint64_t target = 0;
    size_t register_index = 0;
    if (instruction == NULL || facts == NULL) {
        return;
    }
    if (kind == MMAY_BYTECODE_PROVENANCE_ARRAY_INDEX
        && mmay_bytecode_provenance_invalidates_arrays(instruction->opcode)) {
        for (register_index = 0; register_index < register_count;
            register_index += 1) {
            facts[register_index] = MMAY_BYTECODE_PROVENANCE_NONE;
        }
        return;
    }
    if (!mmay_bytecode_opcode_writes_register(instruction->opcode)) {
        return;
    }
    target = instruction->operands[0].unsigned_value;
    if (target >= register_count) {
        return;
    }
    if (kind == MMAY_BYTECODE_PROVENANCE_ARRAY_INDEX) {
        for (register_index = 0; register_index < register_count;
            register_index += 1) {
            if (facts[register_index] == target) {
                facts[register_index] = MMAY_BYTECODE_PROVENANCE_NONE;
            }
        }
        facts[(size_t)target] = instruction->opcode
                == MMAY_OPCODE_ARRAY_REQUIRE_INDEX
            ? instruction->operands[2].unsigned_value
            : MMAY_BYTECODE_PROVENANCE_NONE;
        return;
    }
    facts[(size_t)target] = instruction->opcode
            == MMAY_OPCODE_ENUM_REQUIRE_VARIANT
        ? (uint64_t)instruction->operands[2].signed_value
        : MMAY_BYTECODE_PROVENANCE_NONE;
}

static void mmay_bytecode_provenance_transfer_block(
    const MarimayBytecodeBlock *block,
    uint64_t *facts,
    size_t register_count,
    MarimayBytecodeProvenanceKind kind
) {
    size_t instruction_index = 0;
    if (block == NULL || facts == NULL) {
        return;
    }
    for (instruction_index = 0;
        instruction_index + 1 < block->instructions.length;
        instruction_index += 1) {
        const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
            &block->instructions,
            instruction_index
        );
        mmay_bytecode_provenance_transfer_instruction(
            instruction,
            facts,
            register_count,
            kind
        );
    }
}

/* SIZE_MAX marks a block outside the queue; block_count terminates its linked
 * list. Block zero retains the fixed entry boundary, including on backedges. */
static void mmay_bytecode_provenance_enqueue(
    size_t *pending,
    size_t block_count,
    size_t *head,
    size_t *tail,
    size_t target
) {
    if (target == 0 || target >= block_count || pending[target] != SIZE_MAX) {
        return;
    }
    if (*head == block_count) *head = target;
    else pending[*tail] = target;
    pending[target] = block_count;
    *tail = target;
}

static MarimayBytecodeVerifyStatus mmay_bytecode_provenance_solve(
    const MarimayBytecodeFunction *function,
    const size_t *predecessor_offsets,
    const uint32_t *predecessors,
    uint64_t *incoming,
    uint64_t *outgoing,
    uint64_t *scratch,
    size_t scratch_size,
    size_t *pending,
    MarimayBytecodeProvenanceKind kind,
    size_t available_work,
    size_t *work_used
) {
    size_t register_count = function->register_types.length;
    size_t block_count = function->blocks.length;
    size_t cell_count = 0;
    size_t register_index = 0;
    size_t entry_work = 0;
    size_t work = 0;
    size_t head = block_count > 1 ? 1 : block_count;
    size_t tail = block_count - 1;
    const MarimayBytecodeBlock *entry = mmay_vector_at_const(
        &function->blocks,
        0
    );
    if (!mmay_size_multiply(block_count, register_count, &cell_count)
        || !mmay_size_multiply(
            entry->instructions.length,
            register_count,
            &entry_work
        )) {
        return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    for (register_index = 0; register_index < cell_count;
        register_index += 1) {
        incoming[register_index] = MMAY_BYTECODE_PROVENANCE_TOP;
        outgoing[register_index] = MMAY_BYTECODE_PROVENANCE_TOP;
    }
    for (register_index = 0; register_index < register_count;
        register_index += 1) {
        incoming[register_index] = MMAY_BYTECODE_PROVENANCE_NONE;
        outgoing[register_index] = MMAY_BYTECODE_PROVENANCE_NONE;
    }
    if (!mmay_bytecode_provenance_consume_work(
            &work,
            entry_work,
            available_work
        )) {
        return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    mmay_bytecode_provenance_transfer_block(
        entry,
        outgoing,
        register_count,
        kind
    );
    for (register_index = 0; register_index < block_count; register_index += 1) {
        pending[register_index] = register_index + 1;
    }
    pending[0] = SIZE_MAX;
    while (head != block_count) {
        size_t block_index = head;
        size_t predecessor_index = 0;
        uint64_t *incoming_row = incoming
            + block_index * register_count;
        uint64_t *outgoing_row = outgoing
            + block_index * register_count;
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t transfer_work = 0;
        head = pending[block_index];
        pending[block_index] = SIZE_MAX;
        for (register_index = 0; register_index < register_count;
            register_index += 1) {
            scratch[register_index] = MMAY_BYTECODE_PROVENANCE_TOP;
        }
        for (predecessor_index = predecessor_offsets[block_index];
            predecessor_index < predecessor_offsets[block_index + 1];
            predecessor_index += 1) {
            const uint64_t *predecessor_row = outgoing
                + (size_t)predecessors[predecessor_index]
                    * register_count;
            for (register_index = 0; register_index < register_count;
                register_index += 1) {
                scratch[register_index] = mmay_bytecode_provenance_meet(
                    scratch[register_index],
                    predecessor_row[register_index]
                );
            }
            if (!mmay_bytecode_provenance_consume_work(
                    &work,
                    register_count,
                    available_work
                )) {
                return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        if (register_count != 0
            && memcmp(incoming_row, scratch, scratch_size) != 0) {
            memcpy(incoming_row, scratch, scratch_size);
        }
        if (!mmay_size_multiply(
                block->instructions.length,
                register_count,
                &transfer_work
            ) || !mmay_size_add(transfer_work, 1, &transfer_work)
            || !mmay_bytecode_provenance_consume_work(
                &work,
                transfer_work,
                available_work
            )) {
            return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        mmay_bytecode_provenance_transfer_block(
            block,
            scratch,
            register_count,
            kind
        );
        if (register_count != 0
            && memcmp(outgoing_row, scratch, scratch_size) != 0) {
            const MarimayBytecodeInstruction *terminator =
                mmay_bytecode_block_terminator(block);
            memcpy(outgoing_row, scratch, scratch_size);
            if (terminator->opcode == MMAY_OPCODE_JUMP) {
                mmay_bytecode_provenance_enqueue(
                    pending, block_count, &head, &tail,
                    (size_t)terminator->operands[0].unsigned_value
                );
            } else if (terminator->opcode == MMAY_OPCODE_BRANCH_BOOL) {
                mmay_bytecode_provenance_enqueue(
                    pending, block_count, &head, &tail,
                    (size_t)terminator->operands[1].unsigned_value
                );
                mmay_bytecode_provenance_enqueue(
                    pending, block_count, &head, &tail,
                    (size_t)terminator->operands[2].unsigned_value
                );
            }
        }
    }
    *work_used = work;
    return MMAY_BYTECODE_VERIFY_OK;
}

static MarimayBytecodeVerifyStatus mmay_bytecode_provenance_add_lattice(
    MarimayBytecodeProvenanceAnalysis *analysis,
    const MarimayBytecodeFunction *function,
    const size_t *predecessor_offsets,
    const uint32_t *predecessors,
    MarimayBytecodeProvenanceKind kind,
    size_t available_work,
    size_t *work_used
) {
    uint64_t **incoming = kind == MMAY_BYTECODE_PROVENANCE_ARRAY_INDEX
        ? &analysis->array_incoming
        : &analysis->enum_incoming;
    uint64_t **scratch = kind == MMAY_BYTECODE_PROVENANCE_ARRAY_INDEX
        ? &analysis->array_scratch
        : &analysis->enum_scratch;
    uint64_t *outgoing = NULL;
    size_t *pending = NULL;
    size_t pending_bytes = 0;
    MarimayBytecodeVerifyStatus status = MMAY_BYTECODE_VERIFY_OK;
    if (!mmay_size_multiply(
            analysis->block_count, sizeof(*pending), &pending_bytes
        )) return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
    *incoming = mmay_allocate_array(
        &analysis->allocator,
        analysis->cell_count,
        sizeof(uint64_t)
    );
    outgoing = mmay_allocate_array(
        &analysis->allocator,
        analysis->cell_count,
        sizeof(uint64_t)
    );
    *scratch = mmay_allocate_array(
        &analysis->allocator,
        analysis->register_count,
        sizeof(uint64_t)
    );
    pending = mmay_allocate(&analysis->allocator, pending_bytes);
    if (*incoming == NULL || outgoing == NULL || *scratch == NULL
        || pending == NULL) {
        status = MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED;
    } else {
        status = mmay_bytecode_provenance_solve(
            function,
            predecessor_offsets,
            predecessors,
            *incoming,
            outgoing,
            *scratch,
            analysis->scratch_size,
            pending,
            kind,
            available_work,
            work_used
        );
    }
    mmay_deallocate(&analysis->allocator, pending, pending_bytes);
    mmay_deallocate(
        &analysis->allocator,
        outgoing,
        analysis->cells_size
    );
    return status;
}

MarimayBytecodeVerifyStatus mmay_bytecode_provenance_analyze(
    const MarimayBytecodeFunction *function,
    const size_t *predecessor_offsets,
    const uint32_t *predecessors,
    bool check_array_indices,
    bool check_enum_variants,
    size_t available_work,
    size_t *work_used,
    MarimayBytecodeProvenanceAnalysis **result
) {
    MarimayBytecodeProvenanceAnalysis *analysis = NULL;
    size_t cell_count = 0;
    size_t cells_size = 0;
    size_t lattice_work = 0;
    MarimayBytecodeVerifyStatus status = MMAY_BYTECODE_VERIFY_OK;
    if (work_used == NULL || result == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    *work_used = 0;
    *result = NULL;
    if (!check_array_indices && !check_enum_variants) {
        return MMAY_BYTECODE_VERIFY_OK;
    }
    if (function == NULL || predecessor_offsets == NULL
        || predecessors == NULL || function->register_types.length == 0
        || !mmay_size_multiply(
            function->blocks.length,
            function->register_types.length,
            &cell_count
        ) || !mmay_size_multiply(
            cell_count,
            sizeof(uint64_t),
            &cells_size
        )) {
        return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    analysis = mmay_allocate(&function->allocator, sizeof(*analysis));
    if (analysis == NULL) {
        return MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED;
    }
    memset(analysis, 0, sizeof(*analysis));
    analysis->allocator = function->allocator;
    analysis->register_count = function->register_types.length;
    analysis->block_count = function->blocks.length;
    analysis->cell_count = cell_count;
    analysis->cells_size = cells_size;
    if (!mmay_size_multiply(
            analysis->register_count,
            sizeof(uint64_t),
            &analysis->scratch_size
        )) {
        status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        goto fail;
    }
    if (check_array_indices) {
        status = mmay_bytecode_provenance_add_lattice(
            analysis,
            function,
            predecessor_offsets,
            predecessors,
            MMAY_BYTECODE_PROVENANCE_ARRAY_INDEX,
            available_work,
            &lattice_work
        );
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            goto fail;
        }
        *work_used = lattice_work;
    }
    if (check_enum_variants) {
        status = mmay_bytecode_provenance_add_lattice(
            analysis,
            function,
            predecessor_offsets,
            predecessors,
            MMAY_BYTECODE_PROVENANCE_ENUM_VARIANT,
            available_work - *work_used,
            &lattice_work
        );
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            goto fail;
        }
        *work_used += lattice_work;
    }
    *result = analysis;
    return MMAY_BYTECODE_VERIFY_OK;

fail:
    mmay_bytecode_provenance_dispose(analysis);
    return status;
}

void mmay_bytecode_provenance_begin_block(
    MarimayBytecodeProvenanceAnalysis *analysis,
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

MarimayBytecodeVerifyStatus mmay_bytecode_provenance_validate_instruction(
    const MarimayBytecodeProvenanceAnalysis *analysis,
    const MarimayBytecodeInstruction *instruction
) {
    uint64_t array_register = 0;
    uint64_t index_register = 0;
    if (analysis == NULL || instruction == NULL) {
        return MMAY_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == MMAY_OPCODE_ARRAY_GET_IN_BOUNDS) {
        array_register = instruction->operands[1].unsigned_value;
        index_register = instruction->operands[2].unsigned_value;
    } else {
        array_register = instruction->operands[0].unsigned_value;
        index_register = instruction->operands[1].unsigned_value;
    }
    if ((instruction->opcode == MMAY_OPCODE_ARRAY_SET
            || instruction->opcode == MMAY_OPCODE_ARRAY_GET_IN_BOUNDS
            || instruction->opcode == MMAY_OPCODE_ARRAY_SET_IN_BOUNDS)
        && (analysis->array_scratch == NULL
            || analysis->array_scratch[
                (size_t)array_register
            ] != index_register)) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    if (instruction->opcode == MMAY_OPCODE_ENUM_GET_PAYLOAD) {
        uint64_t selector = (uint64_t)instruction->operands[2].signed_value;
        uint64_t enum_register = instruction->operands[1].unsigned_value;
        if (analysis->enum_scratch == NULL
            || analysis->enum_scratch[(size_t)enum_register]
                != selector >> 32) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
    }
    return MMAY_BYTECODE_VERIFY_OK;
}

MarimayBytecodeVerifyStatus mmay_bytecode_provenance_advance_instruction(
    MarimayBytecodeProvenanceAnalysis *analysis,
    const MarimayBytecodeInstruction *instruction,
    size_t available_work,
    size_t *work_used
) {
    size_t transfer_work = 0;
    if (work_used == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    *work_used = 0;
    if (analysis == NULL || instruction == NULL
        || (!mmay_bytecode_opcode_writes_register(instruction->opcode)
            && (analysis->array_scratch == NULL
                || !mmay_bytecode_provenance_invalidates_arrays(
                    instruction->opcode
                )))) {
        return MMAY_BYTECODE_VERIFY_OK;
    }
    if (!mmay_size_multiply(
            analysis->register_count,
            (analysis->array_scratch != NULL ? 1U : 0U)
                + (analysis->enum_scratch != NULL ? 1U : 0U),
            &transfer_work
        ) || transfer_work > available_work) {
        return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    if (analysis->array_scratch != NULL) {
        mmay_bytecode_provenance_transfer_instruction(
            instruction,
            analysis->array_scratch,
            analysis->register_count,
            MMAY_BYTECODE_PROVENANCE_ARRAY_INDEX
        );
    }
    if (analysis->enum_scratch != NULL) {
        mmay_bytecode_provenance_transfer_instruction(
            instruction,
            analysis->enum_scratch,
            analysis->register_count,
            MMAY_BYTECODE_PROVENANCE_ENUM_VARIANT
        );
    }
    *work_used = transfer_work;
    return MMAY_BYTECODE_VERIFY_OK;
}

void mmay_bytecode_provenance_dispose(
    MarimayBytecodeProvenanceAnalysis *analysis
) {
    MarimayAllocator allocator;
    if (analysis == NULL) {
        return;
    }
    allocator = analysis->allocator;
    mmay_deallocate(
        &allocator,
        analysis->enum_scratch,
        analysis->scratch_size
    );
    mmay_deallocate(
        &allocator,
        analysis->enum_incoming,
        analysis->cells_size
    );
    mmay_deallocate(
        &allocator,
        analysis->array_scratch,
        analysis->scratch_size
    );
    mmay_deallocate(
        &allocator,
        analysis->array_incoming,
        analysis->cells_size
    );
    mmay_deallocate(&allocator, analysis, sizeof(*analysis));
}
