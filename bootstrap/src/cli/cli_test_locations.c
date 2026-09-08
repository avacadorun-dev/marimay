#include "cli_test_locations.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mmay_cli_location_compare(const void *left, const void *right) {
    const MarimayNativePanicLocation *a = left, *b = right;
    int symbol = memcmp(a->symbol.bytes, b->symbol.bytes, sizeof(a->symbol.bytes));
    if (symbol != 0) return symbol;
    if (a->start != b->start) return a->start < b->start ? -1 : 1;
    return a->end < b->end ? -1 : a->end > b->end ? 1 : 0;
}

static bool mmay_cli_location_add(MarimayVector *locations,
    const MarimayCheckedSymbol *fact, MarimaySymbolId symbol, MarimayMirSpan span) {
    MarimayNativePanicLocation location = {0};
    MarimaySourcePosition start, end;
    if (span.start == span.end) return true;
    if (fact == NULL || span.start > span.end
        || !mmay_source_position(fact->snapshot, span.start, &start)
        || !mmay_source_position(fact->snapshot, span.end, &end)) return false;
    location.symbol = symbol;
    location.start = span.start;
    location.end = span.end;
    location.path = mmay_source_path(fact->snapshot);
    location.path_length = mmay_source_path_length(fact->snapshot);
    location.line = start.line;
    location.column = start.column;
    location.end_line = end.line;
    location.end_column = end.column;
    return location.path != NULL && location.path_length != 0
        && mmay_vector_push(locations, &location);
}

bool mmay_cli_test_locations_prepare(const MarimayCheckResult *checked,
    MarimayAllocator allocator, MarimayVector *output) {
    size_t function_index, index, unique = 0;
    if (output == NULL) return false;
    mmay_vector_dispose(output);
    if (checked == NULL || !checked->initialized || !mmay_vector_init(output,
            allocator, sizeof(MarimayNativePanicLocation))) return false;
    for (function_index = 0; function_index < checked->functions.length; function_index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(checked, function_index);
        const MarimayCheckedSymbol *fact;
        size_t block_index;
        if (function == NULL) goto failure;
        if (function->is_external) continue;
        fact = mmay_checked_symbol_by_id(checked, function->mir.symbol);
        if (fact == NULL) fact = mmay_checked_symbol_by_id(checked, function->source_symbol);
        if (fact == NULL) goto failure;
        for (block_index = 0; block_index < function->mir.blocks.length; block_index += 1) {
            const MarimayMirBlock *block = mmay_vector_at_const(&function->mir.blocks, block_index);
            if (block == NULL) goto failure;
            for (index = 0; index < block->instructions.length; index += 1) {
                const MarimayMirInstruction *instruction = mmay_vector_at_const(&block->instructions, index);
                if (instruction == NULL || !mmay_cli_location_add(output, fact, function->mir.symbol, instruction->span)) goto failure;
            }
            if (!mmay_cli_location_add(output, fact, function->mir.symbol, block->terminator.span)) goto failure;
        }
    }
    if (output->length > 1) qsort(output->data, output->length,
        output->element_size, mmay_cli_location_compare);
    for (index = 0; index < output->length; index += 1) {
        const MarimayNativePanicLocation *current = mmay_vector_at_const(output, index);
        MarimayNativePanicLocation *previous = unique == 0 ? NULL : mmay_vector_at(output, unique - 1);
        if (previous == NULL || mmay_cli_location_compare(previous, current) != 0) {
            *(MarimayNativePanicLocation *)mmay_vector_at(output, unique++) = *current;
        }
    }
    output->length = unique;
    return true;
failure:
    mmay_vector_dispose(output);
    return false;
}

bool mmay_cli_test_location_write(const MarimayVector *locations,
    MarimaySymbolId symbol, size_t start, size_t end, MarimayWriter writer) {
    MarimayNativePanicLocation key = {0};
    size_t low = 0, high;
    key.symbol = symbol; key.start = start; key.end = end;
    if (locations == NULL || locations->element_size != sizeof(key)) return false;
    high = locations->length;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const MarimayNativePanicLocation *location = mmay_vector_at_const(locations, middle);
        int compared = mmay_cli_location_compare(location, &key);
        if (compared < 0) low = middle + 1;
        else if (compared > 0) high = middle;
        else {
            char suffix[16 + 12 * sizeof(size_t)];
            int length = snprintf(suffix, sizeof(suffix), ":%zu:%zu-%zu:%zu\n",
                location->line, location->column, location->end_line, location->end_column);
            return length > 0 && (size_t)length < sizeof(suffix)
                && mmay_writer_write_cstr(writer, "  --> ")
                && mmay_writer_write(writer, location->path, location->path_length)
                && mmay_writer_write(writer, suffix, (size_t)length);
        }
    }
    return false;
}
