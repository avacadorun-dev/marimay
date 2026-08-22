#ifndef MMAY_BASE_WRITER_H
#define MMAY_BASE_WRITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef bool (*MarimayWriteFn)(void *context, const char *bytes, size_t length);

typedef struct MarimayWriter {
    void *context;
    MarimayWriteFn write;
} MarimayWriter;

/* The FILE must remain open for every write through the returned value. */
MarimayWriter mmay_file_writer(FILE *file);

bool mmay_writer_write(MarimayWriter writer, const char *bytes, size_t length);
bool mmay_writer_write_cstr(MarimayWriter writer, const char *text);

#endif
