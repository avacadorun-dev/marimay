#include "ruda/base/integer_range.h"

#include <limits.h>
#include <string.h>

#include "ruda/base/checked_i64.h"

static bool ruda_integer_range_consume(
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

static RudaIntegerRange ruda_integer_range_hull(
    RudaIntegerRange left,
    RudaIntegerRange right
) {
    RudaIntegerRange result;
    result.minimum = left.minimum < right.minimum
        ? left.minimum : right.minimum;
    result.maximum = left.maximum > right.maximum
        ? left.maximum : right.maximum;
    return result;
}

static bool ruda_integer_range_add(
    RudaIntegerRange left,
    RudaIntegerRange right,
    RudaIntegerRange *result
) {
    return result != NULL
        && ruda_checked_i64_add(
            left.minimum, right.minimum, &result->minimum
        ) == RUDA_CHECKED_I64_OK
        && ruda_checked_i64_add(
            left.maximum, right.maximum, &result->maximum
        ) == RUDA_CHECKED_I64_OK;
}

static bool ruda_integer_range_subtract(
    RudaIntegerRange left,
    RudaIntegerRange right,
    RudaIntegerRange *result
) {
    return result != NULL
        && ruda_checked_i64_subtract(
            left.minimum, right.maximum, &result->minimum
        ) == RUDA_CHECKED_I64_OK
        && ruda_checked_i64_subtract(
            left.maximum, right.minimum, &result->maximum
        ) == RUDA_CHECKED_I64_OK;
}

static bool ruda_integer_range_multiply(
    RudaIntegerRange left,
    RudaIntegerRange right,
    RudaIntegerRange *result
) {
    int64_t products[4];
    size_t index = 0;
    if (result == NULL
        || ruda_checked_i64_multiply(
            left.minimum, right.minimum, &products[0]
        ) != RUDA_CHECKED_I64_OK
        || ruda_checked_i64_multiply(
            left.minimum, right.maximum, &products[1]
        ) != RUDA_CHECKED_I64_OK
        || ruda_checked_i64_multiply(
            left.maximum, right.minimum, &products[2]
        ) != RUDA_CHECKED_I64_OK
        || ruda_checked_i64_multiply(
            left.maximum, right.maximum, &products[3]
        ) != RUDA_CHECKED_I64_OK) {
        return false;
    }
    result->minimum = products[0];
    result->maximum = products[0];
    for (index = 1; index < 4; index += 1) {
        if (products[index] < result->minimum) {
            result->minimum = products[index];
        }
        if (products[index] > result->maximum) {
            result->maximum = products[index];
        }
    }
    return true;
}

static bool ruda_integer_range_operation(
    RudaIntegerRangeInstructionKind kind,
    RudaIntegerRange left,
    RudaIntegerRange right,
    RudaIntegerRange *result
) {
    switch (kind) {
        case RUDA_INTEGER_RANGE_ADD_CHECKED:
        case RUDA_INTEGER_RANGE_ADD_PROVEN:
            return ruda_integer_range_add(left, right, result);
        case RUDA_INTEGER_RANGE_SUBTRACT_CHECKED:
        case RUDA_INTEGER_RANGE_SUBTRACT_PROVEN:
            return ruda_integer_range_subtract(left, right, result);
        case RUDA_INTEGER_RANGE_MULTIPLY_CHECKED:
        case RUDA_INTEGER_RANGE_MULTIPLY_PROVEN:
            return ruda_integer_range_multiply(left, right, result);
        default:
            return false;
    }
}

static bool ruda_integer_range_register(
    const RudaIntegerRangeProgram *program,
    uint32_t register_id,
    RudaIntegerRange *range
) {
    return program != NULL && range != NULL
        && register_id < program->register_count
        && program->register_bounds(
            program->context, register_id, range
        ) && range->minimum <= range->maximum;
}

static bool ruda_integer_range_transfer(
    const RudaIntegerRangeProgram *program,
    const RudaIntegerRangeInstruction *instruction,
    RudaIntegerRange *facts
) {
    RudaIntegerRange target;
    RudaIntegerRange result;
    if (program == NULL || instruction == NULL || facts == NULL) {
        return false;
    }
    if (instruction->kind == RUDA_INTEGER_RANGE_OTHER
        && instruction->target == RUDA_INTEGER_RANGE_NO_REGISTER) {
        return true;
    }
    if (!ruda_integer_range_register(
            program, instruction->target, &target
        )) {
        return false;
    }
    result = target;
    switch (instruction->kind) {
        case RUDA_INTEGER_RANGE_CONSTANT:
            result.minimum = instruction->constant;
            result.maximum = instruction->constant;
            break;
        case RUDA_INTEGER_RANGE_COPY:
        case RUDA_INTEGER_RANGE_CONVERT_TOTAL:
            if (instruction->first >= program->register_count) {
                return false;
            }
            result = facts[instruction->first];
            if (result.minimum < target.minimum) {
                result.minimum = target.minimum;
            }
            if (result.maximum > target.maximum) {
                result.maximum = target.maximum;
            }
            if (result.minimum > result.maximum) {
                result = target;
            }
            break;
        case RUDA_INTEGER_RANGE_ADD_CHECKED:
        case RUDA_INTEGER_RANGE_ADD_PROVEN:
        case RUDA_INTEGER_RANGE_SUBTRACT_CHECKED:
        case RUDA_INTEGER_RANGE_SUBTRACT_PROVEN:
        case RUDA_INTEGER_RANGE_MULTIPLY_CHECKED:
        case RUDA_INTEGER_RANGE_MULTIPLY_PROVEN:
            if (instruction->first >= program->register_count
                || instruction->second >= program->register_count
                || !ruda_integer_range_operation(
                    instruction->kind,
                    facts[instruction->first], facts[instruction->second],
                    &result
                )) {
                result = target;
            }
            break;
        case RUDA_INTEGER_RANGE_REMAINDER_CHECKED:
            if (instruction->first < program->register_count
                && instruction->second < program->register_count
                && facts[instruction->second].minimum
                    == facts[instruction->second].maximum
                && facts[instruction->second].minimum > 0) {
                int64_t limit = facts[instruction->second].minimum - 1;
                RudaIntegerRange source = facts[instruction->first];
                result.minimum = source.minimum < 0 ? -limit : 0;
                result.maximum = source.maximum > 0 ? limit : 0;
                if (result.minimum < source.minimum) {
                    result.minimum = source.minimum;
                }
                if (result.maximum > source.maximum) {
                    result.maximum = source.maximum;
                }
            }
            break;
        case RUDA_INTEGER_RANGE_ARRAY_LENGTH:
            result.minimum = 0;
            result.maximum = target.maximum;
            break;
        case RUDA_INTEGER_RANGE_OTHER:
        case RUDA_INTEGER_RANGE_LESS:
        case RUDA_INTEGER_RANGE_LESS_EQUAL:
            break;
    }
    if (result.minimum < target.minimum || result.maximum > target.maximum
        || result.minimum > result.maximum) {
        result = target;
    }
    facts[instruction->target] = result;
    return true;
}

static bool ruda_integer_range_edge(
    const RudaIntegerRangeProgram *program,
    size_t predecessor,
    size_t successor,
    const RudaIntegerRange *outgoing,
    RudaIntegerRange *edge
) {
    RudaIntegerRangeTerminator terminator;
    RudaIntegerRangeInstruction condition;
    size_t instruction_count = 0;
    size_t cursor = 0;
    bool true_edge = false;
    if (!program->read_terminator(
            program->context, predecessor, &terminator
        )) {
        return false;
    }
    if (terminator.kind == RUDA_INTEGER_RANGE_TERMINATOR_GOTO) {
        return terminator.target == successor;
    }
    if (terminator.kind != RUDA_INTEGER_RANGE_TERMINATOR_BRANCH
        || (terminator.target != successor
            && terminator.otherwise != successor)
        || !program->block_length(
            program->context, predecessor, &instruction_count
        )) {
        return false;
    }
    true_edge = terminator.target == successor;
    cursor = instruction_count;
    while (cursor != 0) {
        cursor -= 1;
        if (!program->read_instruction(
                program->context, predecessor, cursor, &condition
            )) {
            return false;
        }
        if (condition.target != terminator.condition) {
            continue;
        }
        if ((condition.kind == RUDA_INTEGER_RANGE_LESS
                || condition.kind == RUDA_INTEGER_RANGE_LESS_EQUAL)
            && condition.first < program->register_count
            && condition.second < program->register_count) {
            RudaIntegerRange *left = &edge[condition.first];
            RudaIntegerRange *right = &edge[condition.second];
            if (condition.kind == RUDA_INTEGER_RANGE_LESS) {
                if (true_edge) {
                    if (right->maximum != INT64_MIN
                        && left->maximum >= right->maximum) {
                        left->maximum = right->maximum - 1;
                    }
                    if (left->minimum != INT64_MAX
                        && right->minimum <= left->minimum) {
                        right->minimum = left->minimum + 1;
                    }
                } else {
                    if (left->minimum < right->minimum) {
                        left->minimum = right->minimum;
                    }
                    if (right->maximum > left->maximum) {
                        right->maximum = left->maximum;
                    }
                }
            } else if (true_edge) {
                if (left->maximum > right->maximum) {
                    left->maximum = right->maximum;
                }
                if (right->minimum < left->minimum) {
                    right->minimum = left->minimum;
                }
            } else {
                if (right->minimum != INT64_MAX
                    && left->minimum <= right->minimum) {
                    left->minimum = right->minimum + 1;
                }
                if (left->maximum != INT64_MIN
                    && right->maximum >= left->maximum) {
                    right->maximum = left->maximum - 1;
                }
            }
            if (left->minimum > left->maximum
                || right->minimum > right->maximum) {
                return false;
            }
        }
        break;
    }
    (void)outgoing;
    return true;
}

static bool ruda_integer_range_predecessor(
    const RudaIntegerRangeProgram *program,
    size_t predecessor,
    size_t successor
) {
    RudaIntegerRangeTerminator terminator;
    if (!program->read_terminator(
            program->context, predecessor, &terminator
        )) {
        return false;
    }
    return (terminator.kind == RUDA_INTEGER_RANGE_TERMINATOR_GOTO
            && terminator.target == successor)
        || (terminator.kind == RUDA_INTEGER_RANGE_TERMINATOR_BRANCH
            && (terminator.target == successor
                || terminator.otherwise == successor));
}

RudaIntegerRangeStatus ruda_integer_range_analyze(
    const RudaIntegerRangeProgram *program,
    size_t work_limit,
    size_t *work_used,
    RudaIntegerRangeAnalysis *analysis
) {
    RudaIntegerRange *outgoing = NULL;
    RudaIntegerRange *edge = NULL;
    unsigned char *outgoing_ready = NULL;
    unsigned char *updates = NULL;
    size_t block_index = 0;
    size_t bytes = 0;
    size_t ready_bytes = 0;
    bool changed = true;
    bool narrowing = false;
    RudaIntegerRangeStatus status = RUDA_INTEGER_RANGE_OK;
    if (work_used != NULL) {
        *work_used = 0;
    }
    if (analysis != NULL) {
        memset(analysis, 0, sizeof(*analysis));
    }
    if (program == NULL || work_used == NULL || analysis == NULL
        || program->block_count == 0 || program->register_count == 0
        || program->register_bounds == NULL || program->block_length == NULL
        || program->read_instruction == NULL
        || program->read_terminator == NULL
        || !ruda_size_multiply(
            program->block_count, program->register_count,
            &analysis->cell_count
        ) || !ruda_size_multiply(
            analysis->cell_count, sizeof(RudaIntegerRange), &bytes
        ) || !ruda_size_multiply(
            program->register_count, sizeof(RudaIntegerRange),
            &analysis->scratch_bytes
        )) {
        return RUDA_INTEGER_RANGE_INVALID;
    }
    ready_bytes = program->block_count;
    analysis->program = *program;
    analysis->cell_bytes = bytes;
    analysis->ready_bytes = ready_bytes;
    analysis->incoming = ruda_allocate_array(
        &program->allocator, analysis->cell_count, sizeof(RudaIntegerRange)
    );
    outgoing = ruda_allocate_array(
        &program->allocator, analysis->cell_count, sizeof(RudaIntegerRange)
    );
    analysis->scratch = ruda_allocate_array(
        &program->allocator, program->register_count, sizeof(RudaIntegerRange)
    );
    edge = ruda_allocate_array(
        &program->allocator, program->register_count, sizeof(RudaIntegerRange)
    );
    analysis->incoming_ready = ruda_allocate_array(
        &program->allocator, ready_bytes, sizeof(unsigned char)
    );
    outgoing_ready = ruda_allocate_array(
        &program->allocator, ready_bytes, sizeof(unsigned char)
    );
    updates = ruda_allocate_array(
        &program->allocator, ready_bytes, sizeof(unsigned char)
    );
    if (analysis->incoming == NULL || outgoing == NULL
        || analysis->scratch == NULL || edge == NULL
        || analysis->incoming_ready == NULL || outgoing_ready == NULL
        || updates == NULL) {
        status = RUDA_INTEGER_RANGE_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(analysis->incoming_ready, 0, ready_bytes);
    memset(outgoing_ready, 0, ready_bytes);
    memset(updates, 0, ready_bytes);
    while (changed || !narrowing) {
        if (!changed) {
            narrowing = true;
            changed = true;
        }
        changed = false;
        for (block_index = 0; block_index < program->block_count;
            block_index += 1) {
            size_t predecessor = 0;
            size_t register_index = 0;
            size_t instruction_count = 0;
            size_t instruction_index = 0;
            bool ready = false;
            bool widen = false;
            RudaIntegerRange *incoming_row = analysis->incoming
                + block_index * program->register_count;
            RudaIntegerRange *outgoing_row = outgoing
                + block_index * program->register_count;
            if (!ruda_integer_range_consume(
                    work_used, 1, work_limit
                ) || !program->block_length(
                    program->context, block_index, &instruction_count
                )) {
                status = *work_used >= work_limit
                    ? RUDA_INTEGER_RANGE_LIMIT_REACHED
                    : RUDA_INTEGER_RANGE_INVALID;
                goto cleanup;
            }
            if (block_index == 0) {
                for (register_index = 0;
                    register_index < program->register_count;
                    register_index += 1) {
                    if (!ruda_integer_range_register(
                            program, (uint32_t)register_index,
                            &analysis->scratch[register_index]
                        )) {
                        status = RUDA_INTEGER_RANGE_INVALID;
                        goto cleanup;
                    }
                }
                ready = true;
            } else {
                for (predecessor = 0;
                    predecessor < program->block_count;
                    predecessor += 1) {
                    const RudaIntegerRange *predecessor_row = outgoing
                        + predecessor * program->register_count;
                    if (!outgoing_ready[predecessor]
                        || !ruda_integer_range_predecessor(
                            program, predecessor, block_index
                        )) {
                        continue;
                    }
                    if (predecessor >= block_index) {
                        widen = true;
                    }
                    memcpy(edge, predecessor_row, analysis->scratch_bytes);
                    if (!ruda_integer_range_edge(
                            program, predecessor, block_index,
                            predecessor_row, edge
                        )) {
                        continue;
                    }
                    if (!ready) {
                        memcpy(
                            analysis->scratch, edge,
                            analysis->scratch_bytes
                        );
                        ready = true;
                    } else {
                        for (register_index = 0;
                            register_index < program->register_count;
                            register_index += 1) {
                            analysis->scratch[register_index] =
                                ruda_integer_range_hull(
                                    analysis->scratch[register_index],
                                    edge[register_index]
                                );
                        }
                    }
                    if (!ruda_integer_range_consume(
                            work_used, program->register_count, work_limit
                        )) {
                        status = RUDA_INTEGER_RANGE_LIMIT_REACHED;
                        goto cleanup;
                    }
                }
            }
            if (!ready) {
                continue;
            }
            if (analysis->incoming_ready[block_index] && !narrowing) {
                for (register_index = 0;
                    register_index < program->register_count;
                    register_index += 1) {
                    analysis->scratch[register_index] =
                        ruda_integer_range_hull(
                            incoming_row[register_index],
                            analysis->scratch[register_index]
                        );
                }
            }
            if (analysis->incoming_ready[block_index]
                && !narrowing && widen && updates[block_index] >= 2) {
                for (register_index = 0;
                    register_index < program->register_count;
                    register_index += 1) {
                    RudaIntegerRange base;
                    if (!ruda_integer_range_register(
                            program, (uint32_t)register_index, &base
                        )) {
                        status = RUDA_INTEGER_RANGE_INVALID;
                        goto cleanup;
                    }
                    if (analysis->scratch[register_index].minimum
                        < incoming_row[register_index].minimum) {
                        analysis->scratch[register_index].minimum = base.minimum;
                    }
                    if (analysis->scratch[register_index].maximum
                        > incoming_row[register_index].maximum) {
                        analysis->scratch[register_index].maximum = base.maximum;
                    }
                }
            }
            if (!analysis->incoming_ready[block_index]
                || memcmp(
                    incoming_row, analysis->scratch,
                    analysis->scratch_bytes
                ) != 0) {
                memcpy(
                    incoming_row, analysis->scratch,
                    analysis->scratch_bytes
                );
                analysis->incoming_ready[block_index] = 1;
                if (updates[block_index] != UCHAR_MAX) {
                    updates[block_index] += 1;
                }
                changed = true;
            }
            memcpy(analysis->scratch, incoming_row, analysis->scratch_bytes);
            for (instruction_index = 0;
                instruction_index < instruction_count;
                instruction_index += 1) {
                RudaIntegerRangeInstruction instruction;
                if (!ruda_integer_range_consume(
                        work_used, 1, work_limit
                    ) || !program->read_instruction(
                        program->context, block_index, instruction_index,
                        &instruction
                    ) || !ruda_integer_range_transfer(
                        program, &instruction, analysis->scratch
                    )) {
                    status = *work_used >= work_limit
                        ? RUDA_INTEGER_RANGE_LIMIT_REACHED
                        : RUDA_INTEGER_RANGE_INVALID;
                    goto cleanup;
                }
            }
            if (!outgoing_ready[block_index]
                || memcmp(
                    outgoing_row, analysis->scratch,
                    analysis->scratch_bytes
                ) != 0) {
                memcpy(
                    outgoing_row, analysis->scratch,
                    analysis->scratch_bytes
                );
                outgoing_ready[block_index] = 1;
                changed = true;
            }
        }
    }
cleanup:
    ruda_deallocate(&program->allocator, outgoing, bytes);
    ruda_deallocate(
        &program->allocator, edge, analysis->scratch_bytes
    );
    ruda_deallocate(&program->allocator, outgoing_ready, ready_bytes);
    ruda_deallocate(&program->allocator, updates, ready_bytes);
    if (status != RUDA_INTEGER_RANGE_OK) {
        ruda_integer_range_analysis_dispose(analysis);
    }
    return status;
}

void ruda_integer_range_analysis_dispose(
    RudaIntegerRangeAnalysis *analysis
) {
    if (analysis == NULL) {
        return;
    }
    ruda_deallocate(
        &analysis->program.allocator, analysis->incoming,
        analysis->cell_bytes
    );
    ruda_deallocate(
        &analysis->program.allocator, analysis->scratch,
        analysis->scratch_bytes
    );
    ruda_deallocate(
        &analysis->program.allocator, analysis->incoming_ready,
        analysis->ready_bytes
    );
    memset(analysis, 0, sizeof(*analysis));
}

bool ruda_integer_range_begin_block(
    RudaIntegerRangeAnalysis *analysis,
    size_t block_index
) {
    if (analysis == NULL || block_index >= analysis->program.block_count
        || analysis->incoming_ready[block_index] == 0) {
        return false;
    }
    memcpy(
        analysis->scratch,
        analysis->incoming
            + block_index * analysis->program.register_count,
        analysis->scratch_bytes
    );
    return true;
}

bool ruda_integer_range_proves_instruction(
    const RudaIntegerRangeAnalysis *analysis,
    const RudaIntegerRangeInstruction *instruction
) {
    RudaIntegerRange result;
    RudaIntegerRange target;
    return analysis != NULL && instruction != NULL
        && instruction->target < analysis->program.register_count
        && instruction->first < analysis->program.register_count
        && instruction->second < analysis->program.register_count
        && analysis->program.register_bounds(
            analysis->program.context, instruction->target, &target
        ) && ruda_integer_range_operation(
            instruction->kind,
            analysis->scratch[instruction->first],
            analysis->scratch[instruction->second],
            &result
        ) && result.minimum >= target.minimum
        && result.maximum <= target.maximum;
}

bool ruda_integer_range_advance(
    RudaIntegerRangeAnalysis *analysis,
    const RudaIntegerRangeInstruction *instruction
) {
    return analysis != NULL && ruda_integer_range_transfer(
        &analysis->program, instruction, analysis->scratch
    );
}
