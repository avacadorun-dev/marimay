#include "mmay/base/integer_range.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/checked_i64.h"

static bool mmay_integer_range_consume(
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

static MarimayIntegerRange mmay_integer_range_hull(
    MarimayIntegerRange left,
    MarimayIntegerRange right
) {
    MarimayIntegerRange result;
    result.minimum = left.minimum < right.minimum
        ? left.minimum : right.minimum;
    result.maximum = left.maximum > right.maximum
        ? left.maximum : right.maximum;
    return result;
}

static bool mmay_integer_range_add(
    MarimayIntegerRange left,
    MarimayIntegerRange right,
    MarimayIntegerRange *result
) {
    return result != NULL
        && mmay_checked_i64_add(
            left.minimum, right.minimum, &result->minimum
        ) == MMAY_CHECKED_I64_OK
        && mmay_checked_i64_add(
            left.maximum, right.maximum, &result->maximum
        ) == MMAY_CHECKED_I64_OK;
}

static bool mmay_integer_range_subtract(
    MarimayIntegerRange left,
    MarimayIntegerRange right,
    MarimayIntegerRange *result
) {
    return result != NULL
        && mmay_checked_i64_subtract(
            left.minimum, right.maximum, &result->minimum
        ) == MMAY_CHECKED_I64_OK
        && mmay_checked_i64_subtract(
            left.maximum, right.minimum, &result->maximum
        ) == MMAY_CHECKED_I64_OK;
}

static bool mmay_integer_range_multiply(
    MarimayIntegerRange left,
    MarimayIntegerRange right,
    MarimayIntegerRange *result
) {
    int64_t products[4];
    size_t index = 0;
    if (result == NULL
        || mmay_checked_i64_multiply(
            left.minimum, right.minimum, &products[0]
        ) != MMAY_CHECKED_I64_OK
        || mmay_checked_i64_multiply(
            left.minimum, right.maximum, &products[1]
        ) != MMAY_CHECKED_I64_OK
        || mmay_checked_i64_multiply(
            left.maximum, right.minimum, &products[2]
        ) != MMAY_CHECKED_I64_OK
        || mmay_checked_i64_multiply(
            left.maximum, right.maximum, &products[3]
        ) != MMAY_CHECKED_I64_OK) {
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

static bool mmay_integer_range_operation(
    MarimayIntegerRangeInstructionKind kind,
    MarimayIntegerRange left,
    MarimayIntegerRange right,
    MarimayIntegerRange *result
) {
    switch (kind) {
        case MMAY_INTEGER_RANGE_ADD_CHECKED:
        case MMAY_INTEGER_RANGE_ADD_PROVEN:
            return mmay_integer_range_add(left, right, result);
        case MMAY_INTEGER_RANGE_SUBTRACT_CHECKED:
        case MMAY_INTEGER_RANGE_SUBTRACT_PROVEN:
            return mmay_integer_range_subtract(left, right, result);
        case MMAY_INTEGER_RANGE_MULTIPLY_CHECKED:
        case MMAY_INTEGER_RANGE_MULTIPLY_PROVEN:
            return mmay_integer_range_multiply(left, right, result);
        default:
            return false;
    }
}

static bool mmay_integer_range_register(
    const MarimayIntegerRangeProgram *program,
    uint32_t register_id,
    MarimayIntegerRange *range
) {
    return program != NULL && range != NULL
        && register_id < program->register_count
        && program->register_bounds(
            program->context, register_id, range
        ) && range->minimum <= range->maximum;
}

static bool mmay_integer_range_transfer(
    const MarimayIntegerRangeProgram *program,
    const MarimayIntegerRangeInstruction *instruction,
    MarimayIntegerRange *facts
) {
    MarimayIntegerRange target;
    MarimayIntegerRange result;
    if (program == NULL || instruction == NULL || facts == NULL) {
        return false;
    }
    if (instruction->kind == MMAY_INTEGER_RANGE_OTHER
        && instruction->target == MMAY_INTEGER_RANGE_NO_REGISTER) {
        return true;
    }
    if (!mmay_integer_range_register(
            program, instruction->target, &target
        )) {
        return false;
    }
    result = target;
    switch (instruction->kind) {
        case MMAY_INTEGER_RANGE_CONSTANT:
            result.minimum = instruction->constant;
            result.maximum = instruction->constant;
            break;
        case MMAY_INTEGER_RANGE_COPY:
        case MMAY_INTEGER_RANGE_CONVERT_TOTAL:
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
        case MMAY_INTEGER_RANGE_ADD_CHECKED:
        case MMAY_INTEGER_RANGE_ADD_PROVEN:
        case MMAY_INTEGER_RANGE_SUBTRACT_CHECKED:
        case MMAY_INTEGER_RANGE_SUBTRACT_PROVEN:
        case MMAY_INTEGER_RANGE_MULTIPLY_CHECKED:
        case MMAY_INTEGER_RANGE_MULTIPLY_PROVEN:
            if (instruction->first >= program->register_count
                || instruction->second >= program->register_count
                || !mmay_integer_range_operation(
                    instruction->kind,
                    facts[instruction->first], facts[instruction->second],
                    &result
                )) {
                result = target;
            }
            break;
        case MMAY_INTEGER_RANGE_REMAINDER_CHECKED:
            if (instruction->first < program->register_count
                && instruction->second < program->register_count
                && facts[instruction->second].minimum
                    == facts[instruction->second].maximum
                && facts[instruction->second].minimum > 0) {
                int64_t limit = facts[instruction->second].minimum - 1;
                MarimayIntegerRange source = facts[instruction->first];
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
        case MMAY_INTEGER_RANGE_ARRAY_LENGTH:
            result.minimum = 0;
            result.maximum = target.maximum;
            break;
        case MMAY_INTEGER_RANGE_OTHER:
        case MMAY_INTEGER_RANGE_LESS:
        case MMAY_INTEGER_RANGE_LESS_EQUAL:
            break;
    }
    if (result.minimum < target.minimum || result.maximum > target.maximum
        || result.minimum > result.maximum) {
        result = target;
    }
    facts[instruction->target] = result;
    return true;
}

static bool mmay_integer_range_edge(
    const MarimayIntegerRangeProgram *program,
    size_t predecessor,
    size_t successor,
    const MarimayIntegerRange *outgoing,
    MarimayIntegerRange *edge
) {
    MarimayIntegerRangeTerminator terminator;
    MarimayIntegerRangeInstruction condition;
    size_t instruction_count = 0;
    size_t cursor = 0;
    bool true_edge = false;
    if (!program->read_terminator(
            program->context, predecessor, &terminator
        )) {
        return false;
    }
    if (terminator.kind == MMAY_INTEGER_RANGE_TERMINATOR_GOTO) {
        return terminator.target == successor;
    }
    if (terminator.kind != MMAY_INTEGER_RANGE_TERMINATOR_BRANCH
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
        if ((condition.kind == MMAY_INTEGER_RANGE_LESS
                || condition.kind == MMAY_INTEGER_RANGE_LESS_EQUAL)
            && condition.first < program->register_count
            && condition.second < program->register_count) {
            MarimayIntegerRange *left = &edge[condition.first];
            MarimayIntegerRange *right = &edge[condition.second];
            if (condition.kind == MMAY_INTEGER_RANGE_LESS) {
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

static bool mmay_integer_range_predecessor(
    const MarimayIntegerRangeProgram *program,
    size_t predecessor,
    size_t successor
) {
    MarimayIntegerRangeTerminator terminator;
    if (!program->read_terminator(
            program->context, predecessor, &terminator
        )) {
        return false;
    }
    return (terminator.kind == MMAY_INTEGER_RANGE_TERMINATOR_GOTO
            && terminator.target == successor)
        || (terminator.kind == MMAY_INTEGER_RANGE_TERMINATOR_BRANCH
            && (terminator.target == successor
                || terminator.otherwise == successor));
}

MarimayIntegerRangeStatus mmay_integer_range_analyze(
    const MarimayIntegerRangeProgram *program,
    size_t work_limit,
    size_t *work_used,
    MarimayIntegerRangeAnalysis *analysis
) {
    MarimayIntegerRange *outgoing = NULL;
    MarimayIntegerRange *edge = NULL;
    unsigned char *outgoing_ready = NULL;
    unsigned char *updates = NULL;
    size_t block_index = 0;
    size_t bytes = 0;
    size_t ready_bytes = 0;
    bool changed = true;
    bool narrowing = false;
    MarimayIntegerRangeStatus status = MMAY_INTEGER_RANGE_OK;
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
        || !mmay_size_multiply(
            program->block_count, program->register_count,
            &analysis->cell_count
        ) || !mmay_size_multiply(
            analysis->cell_count, sizeof(MarimayIntegerRange), &bytes
        ) || !mmay_size_multiply(
            program->register_count, sizeof(MarimayIntegerRange),
            &analysis->scratch_bytes
        )) {
        return MMAY_INTEGER_RANGE_INVALID;
    }
    ready_bytes = program->block_count;
    analysis->program = *program;
    analysis->cell_bytes = bytes;
    analysis->ready_bytes = ready_bytes;
    analysis->incoming = mmay_allocate_array(
        &program->allocator, analysis->cell_count, sizeof(MarimayIntegerRange)
    );
    outgoing = mmay_allocate_array(
        &program->allocator, analysis->cell_count, sizeof(MarimayIntegerRange)
    );
    analysis->scratch = mmay_allocate_array(
        &program->allocator, program->register_count, sizeof(MarimayIntegerRange)
    );
    edge = mmay_allocate_array(
        &program->allocator, program->register_count, sizeof(MarimayIntegerRange)
    );
    analysis->incoming_ready = mmay_allocate_array(
        &program->allocator, ready_bytes, sizeof(unsigned char)
    );
    outgoing_ready = mmay_allocate_array(
        &program->allocator, ready_bytes, sizeof(unsigned char)
    );
    updates = mmay_allocate_array(
        &program->allocator, ready_bytes, sizeof(unsigned char)
    );
    if (analysis->incoming == NULL || outgoing == NULL
        || analysis->scratch == NULL || edge == NULL
        || analysis->incoming_ready == NULL || outgoing_ready == NULL
        || updates == NULL) {
        status = MMAY_INTEGER_RANGE_ALLOCATION_FAILED;
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
            MarimayIntegerRange *incoming_row = analysis->incoming
                + block_index * program->register_count;
            MarimayIntegerRange *outgoing_row = outgoing
                + block_index * program->register_count;
            if (!mmay_integer_range_consume(
                    work_used, 1, work_limit
                ) || !program->block_length(
                    program->context, block_index, &instruction_count
                )) {
                status = *work_used >= work_limit
                    ? MMAY_INTEGER_RANGE_LIMIT_REACHED
                    : MMAY_INTEGER_RANGE_INVALID;
                goto cleanup;
            }
            if (block_index == 0) {
                for (register_index = 0;
                    register_index < program->register_count;
                    register_index += 1) {
                    if (!mmay_integer_range_register(
                            program, (uint32_t)register_index,
                            &analysis->scratch[register_index]
                        )) {
                        status = MMAY_INTEGER_RANGE_INVALID;
                        goto cleanup;
                    }
                }
                ready = true;
            } else {
                for (predecessor = 0;
                    predecessor < program->block_count;
                    predecessor += 1) {
                    const MarimayIntegerRange *predecessor_row = outgoing
                        + predecessor * program->register_count;
                    if (!outgoing_ready[predecessor]
                        || !mmay_integer_range_predecessor(
                            program, predecessor, block_index
                        )) {
                        continue;
                    }
                    if (predecessor >= block_index) {
                        widen = true;
                    }
                    memcpy(edge, predecessor_row, analysis->scratch_bytes);
                    if (!mmay_integer_range_edge(
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
                                mmay_integer_range_hull(
                                    analysis->scratch[register_index],
                                    edge[register_index]
                                );
                        }
                    }
                    if (!mmay_integer_range_consume(
                            work_used, program->register_count, work_limit
                        )) {
                        status = MMAY_INTEGER_RANGE_LIMIT_REACHED;
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
                        mmay_integer_range_hull(
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
                    MarimayIntegerRange base;
                    if (!mmay_integer_range_register(
                            program, (uint32_t)register_index, &base
                        )) {
                        status = MMAY_INTEGER_RANGE_INVALID;
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
                MarimayIntegerRangeInstruction instruction;
                if (!mmay_integer_range_consume(
                        work_used, 1, work_limit
                    ) || !program->read_instruction(
                        program->context, block_index, instruction_index,
                        &instruction
                    ) || !mmay_integer_range_transfer(
                        program, &instruction, analysis->scratch
                    )) {
                    status = *work_used >= work_limit
                        ? MMAY_INTEGER_RANGE_LIMIT_REACHED
                        : MMAY_INTEGER_RANGE_INVALID;
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
    mmay_deallocate(&program->allocator, outgoing, bytes);
    mmay_deallocate(
        &program->allocator, edge, analysis->scratch_bytes
    );
    mmay_deallocate(&program->allocator, outgoing_ready, ready_bytes);
    mmay_deallocate(&program->allocator, updates, ready_bytes);
    if (status != MMAY_INTEGER_RANGE_OK) {
        mmay_integer_range_analysis_dispose(analysis);
    }
    return status;
}

void mmay_integer_range_analysis_dispose(
    MarimayIntegerRangeAnalysis *analysis
) {
    if (analysis == NULL) {
        return;
    }
    mmay_deallocate(
        &analysis->program.allocator, analysis->incoming,
        analysis->cell_bytes
    );
    mmay_deallocate(
        &analysis->program.allocator, analysis->scratch,
        analysis->scratch_bytes
    );
    mmay_deallocate(
        &analysis->program.allocator, analysis->incoming_ready,
        analysis->ready_bytes
    );
    memset(analysis, 0, sizeof(*analysis));
}

bool mmay_integer_range_begin_block(
    MarimayIntegerRangeAnalysis *analysis,
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

bool mmay_integer_range_proves_instruction(
    const MarimayIntegerRangeAnalysis *analysis,
    const MarimayIntegerRangeInstruction *instruction
) {
    MarimayIntegerRange result;
    MarimayIntegerRange target;
    return analysis != NULL && instruction != NULL
        && instruction->target < analysis->program.register_count
        && instruction->first < analysis->program.register_count
        && instruction->second < analysis->program.register_count
        && analysis->program.register_bounds(
            analysis->program.context, instruction->target, &target
        ) && mmay_integer_range_operation(
            instruction->kind,
            analysis->scratch[instruction->first],
            analysis->scratch[instruction->second],
            &result
        ) && result.minimum >= target.minimum
        && result.maximum <= target.maximum;
}

bool mmay_integer_range_advance(
    MarimayIntegerRangeAnalysis *analysis,
    const MarimayIntegerRangeInstruction *instruction
) {
    return analysis != NULL && mmay_integer_range_transfer(
        &analysis->program, instruction, analysis->scratch
    );
}
