#ifndef RUDA_BASE_WRITER_H
#define RUDA_BASE_WRITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef bool (*RudaWriteFn)(void *context, const char *bytes, size_t length);

typedef struct RudaWriter {
    void *context;
    RudaWriteFn write;
} RudaWriter;

/* The FILE must remain open for every write through the returned value. */
RudaWriter ruda_file_writer(FILE *file);

bool ruda_writer_write(RudaWriter writer, const char *bytes, size_t length);
bool ruda_writer_write_cstr(RudaWriter writer, const char *text);

#endif
