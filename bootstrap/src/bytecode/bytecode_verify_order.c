#include "bytecode_verify_order.h"

#include <string.h>

#include "bytecode_internal.h"

MarimayBytecodeVerifyStatus mmay_bytecode_verify_order(
    const MarimayBytecodeFunction *function,
    unsigned char *visited,
    size_t *stack,
    uint32_t *order,
    size_t available_work,
    size_t *work_used
) {
    size_t blocks = function->blocks.length;
    size_t depth = 1;
    size_t remaining = blocks;
    *work_used = 0;
    memset(visited, 0, blocks);
    stack[0] = 0;
    visited[0] = 1;
    while (depth != 0) {
        size_t current = stack[depth - 1];
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks, current
        );
        const MarimayBytecodeInstruction *terminator =
            mmay_bytecode_block_terminator(block);
        size_t edges = terminator->opcode == MMAY_OPCODE_JUMP ? 1
            : terminator->opcode == MMAY_OPCODE_BRANCH_BOOL ? 2 : 0;
        size_t edge = (size_t)visited[current] - 1;
        if (*work_used >= available_work) {
            return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        *work_used += 1;
        if (edge < edges) {
            size_t operand = terminator->opcode == MMAY_OPCODE_JUMP
                ? 0 : edge + 1;
            size_t target = (size_t)terminator->operands[operand].unsigned_value;
            /* A nonzero byte both marks discovery and remembers the next
             * edge. Resuming a parent never scans an edge twice. */
            visited[current] += 1;
            if (visited[target] == 0) {
                visited[target] = 1;
                stack[depth] = target;
                depth += 1;
            }
        } else {
            remaining -= 1;
            order[remaining] = (uint32_t)current;
            depth -= 1;
        }
    }
    return remaining == 0 ? MMAY_BYTECODE_VERIFY_OK
        : MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
}
