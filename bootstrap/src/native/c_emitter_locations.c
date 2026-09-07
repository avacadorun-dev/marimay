#include "c_emitter_locations.h"
#include "c_emitter_internal.h"

#include <string.h>
#include "mmay/base/utf8.h"

bool mmay_native_c_locations_valid(const MarimayNativePanicLocation *locations,
    size_t count, const MarimayMirFunction *const *functions, size_t function_count) {
    size_t index;
    if ((locations == NULL) != (count == 0)) return false;
    for (index = 0; index < count; index += 1) {
        const MarimayNativePanicLocation *current = &locations[index];
        if (current->path == NULL || current->path_length == 0
            || memchr(current->path, 0, current->path_length) != NULL
            || !mmay_utf8_validate(current->path, current->path_length, NULL)
            || current->start >= current->end || current->line == 0 || current->column == 0
            || current->end_line < current->line || current->end_column == 0
            || (current->end_line == current->line && current->end_column <= current->column)
            || mmay_native_c_find(functions, function_count, current->symbol) == NULL) return false;
        if (index != 0) {
            const MarimayNativePanicLocation *previous = &locations[index - 1];
            int symbol = mmay_native_c_symbol_compare(previous->symbol, current->symbol);
            if (symbol == 0 && (previous->path_length != current->path_length
                    || memcmp(previous->path, current->path, current->path_length) != 0)) return false;
            if (symbol > 0 || (symbol == 0 && (previous->start > current->start
                    || (previous->start == current->start && previous->end >= current->end)))) return false;
        }
    }
    return true;
}

static bool mmay_native_c_location_path(MarimayNativeCBuffer *buffer,
    const MarimayNativePanicLocation *location) {
    size_t byte;
    if (!mmay_native_c_write(buffer, "static const char mmay_g_source_")
        || !mmay_native_c_symbol_hex(buffer, location->symbol)
        || !mmay_native_c_write(buffer, "[] = \"")) return false;
    for (byte = 0; byte < location->path_length; byte += 1) {
        if (!mmay_native_c_write(buffer, "\\x%02x", (unsigned)(unsigned char)location->path[byte])) return false;
    }
    return mmay_native_c_write(buffer, "\";\n");
}

static bool mmay_native_c_location_row(MarimayNativeCBuffer *buffer,
    const MarimayNativePanicLocation *location) {
    return mmay_native_c_write(buffer, "{ ")
        && mmay_native_c_symbol_data(buffer, location->symbol)
        && mmay_native_c_write(buffer, ", %zu, %zu, mmay_g_source_", location->start, location->end)
        && mmay_native_c_symbol_hex(buffer, location->symbol)
        && mmay_native_c_write(buffer, ", %zu, %zu, %zu, %zu, %zu },\n",
            location->path_length, location->line, location->column, location->end_line, location->end_column);
}

bool mmay_native_c_emit_locations(MarimayNativeCBuffer *buffer,
    const MarimayNativePanicLocation *locations, size_t count) {
    size_t index;
    if (count == 0) return mmay_native_c_write(buffer,
        "static bool mmay_g_print_panic_location(const mmay_g_panic *p) { (void)p; return true; }\n");
    for (index = 0; index < count; index += 1) {
        if ((index == 0 || !mmay_symbol_id_equal(locations[index - 1].symbol, locations[index].symbol))
            && !mmay_native_c_location_path(buffer, &locations[index])) return false;
    }
    if (!mmay_native_c_write(buffer,
            "typedef struct { const unsigned char *symbol; size_t start, end; const char *path; size_t path_length, line, column, end_line, end_column; } mmay_g_panic_location;\n"
            "static const mmay_g_panic_location mmay_g_locations[] = {\n")) return false;
    for (index = 0; index < count; index += 1) {
        if (!mmay_native_c_location_row(buffer, &locations[index])) return false;
    }
    return mmay_native_c_write(buffer,
        "};\n"
        "static bool mmay_g_print_panic_location(const mmay_g_panic *p) {\n"
        "  size_t low = 0, high = sizeof(mmay_g_locations) / sizeof(mmay_g_locations[0]);\n"
        "  if (p == NULL || p->symbol == NULL) return false;\n"
        "  while (low < high) {\n"
        "    size_t middle = low + (high - low) / 2; const mmay_g_panic_location *location = &mmay_g_locations[middle];\n"
        "    int compared = memcmp(location->symbol, p->symbol, 16);\n"
        "    if (compared == 0) compared = location->start < p->start ? -1 : location->start > p->start ? 1 : location->end < p->end ? -1 : location->end > p->end ? 1 : 0;\n"
        "    if (compared < 0) low = middle + 1;\n"
        "    else if (compared > 0) high = middle;\n"
        "    else return fputs(\"  --> \", stderr) != EOF && fwrite(location->path, 1, location->path_length, stderr) == location->path_length && fprintf(stderr, \":%%zu:%%zu-%%zu:%%zu\\n\", location->line, location->column, location->end_line, location->end_column) >= 0 && fflush(stderr) == 0;\n"
        "  }\n  return false;\n}\n");
}
