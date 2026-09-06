#include "bytecode_verify_assignments.h"

#include <string.h>

#include "bytecode_internal.h"

static bool mmay_bytecode_assignment_consume_work(
    size_t *work, size_t amount, size_t limit
) {
    size_t next = 0;
    if (!mmay_size_add(*work, amount, &next) || next > limit) return false;
    *work = next;
    return true;
}

static void mmay_bytecode_assignment_transfer(
    const MarimayBytecodeBlock *block,
    unsigned char *assigned,
    size_t register_count
) {
    size_t index = 0;
    for (index = 0; index + 1 < block->instructions.length; index += 1) {
        const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
            &block->instructions, index
        );
        if (mmay_bytecode_opcode_writes_register(instruction->opcode)
            && instruction->operands[0].unsigned_value < register_count) {
            assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
        }
    }
}

/* SIZE_MAX is not queued; block_count is the list terminator. The fixed
 * entry boundary is never updated by a backedge. */
static void mmay_bytecode_assignment_enqueue(
    size_t *pending, size_t block_count, size_t *head, size_t *tail,
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

MarimayBytecodeVerifyStatus mmay_bytecode_verify_assignments(
    const MarimayBytecodeFunction *function,
    const size_t *predecessor_offsets,
    const uint32_t *predecessors,
    unsigned char *incoming,
    unsigned char *outgoing,
    unsigned char *scratch,
    size_t *pending,
    size_t available_work,
    size_t *work_used
) {
    size_t registers = function->register_types.length;
    size_t blocks = function->blocks.length;
    size_t cells = 0;
    size_t work = 0;
    size_t index = 0;
    size_t head = blocks > 1 ? 1 : blocks;
    size_t tail = blocks - 1;
    const MarimayBytecodeBlock *entry = mmay_vector_at_const(
        &function->blocks, 0
    );
    *work_used = 0;
    if (!mmay_size_multiply(blocks, registers, &cells)) {
        return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    memset(incoming, 1, cells);
    memset(outgoing, 1, cells);
    memset(incoming, 0, registers);
    memset(incoming, 1, function->parameter_count);
    memcpy(outgoing, incoming, registers);
    mmay_bytecode_assignment_transfer(entry, outgoing, registers);
    for (index = 0; index < blocks; index += 1) pending[index] = index + 1;
    pending[0] = SIZE_MAX;
    while (head != blocks) {
        size_t block_index = head;
        size_t predecessor_index = 0;
        size_t transfer_work = 0;
        unsigned char *incoming_row = incoming + block_index * registers;
        unsigned char *outgoing_row = outgoing + block_index * registers;
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        head = pending[block_index];
        pending[block_index] = SIZE_MAX;
        if (!mmay_size_multiply(registers, 2, &transfer_work)
            || !mmay_bytecode_assignment_consume_work(
                &work, transfer_work, available_work
            )) return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        memset(scratch, 1, registers);
        for (predecessor_index = predecessor_offsets[block_index];
            predecessor_index < predecessor_offsets[block_index + 1];
            predecessor_index += 1) {
            const unsigned char *row = outgoing
                + (size_t)predecessors[predecessor_index] * registers;
            if (!mmay_bytecode_assignment_consume_work(
                    &work, registers, available_work
                )) return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            for (index = 0; index < registers; index += 1) {
                scratch[index] = (unsigned char)(scratch[index] & row[index]);
            }
        }
        memcpy(incoming_row, scratch, registers);
        mmay_bytecode_assignment_transfer(block, scratch, registers);
        if (memcmp(outgoing_row, scratch, registers) != 0) {
            const MarimayBytecodeInstruction *terminator =
                mmay_bytecode_block_terminator(block);
            memcpy(outgoing_row, scratch, registers);
            if (terminator->opcode == MMAY_OPCODE_JUMP) {
                mmay_bytecode_assignment_enqueue(
                    pending, blocks, &head, &tail,
                    (size_t)terminator->operands[0].unsigned_value
                );
            } else if (terminator->opcode == MMAY_OPCODE_BRANCH_BOOL) {
                mmay_bytecode_assignment_enqueue(
                    pending, blocks, &head, &tail,
                    (size_t)terminator->operands[1].unsigned_value
                );
                mmay_bytecode_assignment_enqueue(
                    pending, blocks, &head, &tail,
                    (size_t)terminator->operands[2].unsigned_value
                );
            }
        }
    }
    *work_used = work;
    return MMAY_BYTECODE_VERIFY_OK;
}
