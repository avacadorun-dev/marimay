#include "ruda/base/writer.h"

#include <string.h>

static bool ruda_file_write(void *context, const char *bytes, size_t length) {
    FILE *file = context;
    if (file == NULL) {
        return false;
    }
    return fwrite(bytes, 1, length, file) == length;
}

RudaWriter ruda_file_writer(FILE *file) {
    RudaWriter writer;
    writer.context = file;
    writer.write = ruda_file_write;
    return writer;
}

bool ruda_writer_write(RudaWriter writer, const char *bytes, size_t length) {
    if (writer.write == NULL || (bytes == NULL && length != 0)) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    return writer.write(writer.context, bytes, length);
}

bool ruda_writer_write_cstr(RudaWriter writer, const char *text) {
    if (text == NULL) {
        return false;
    }
    return ruda_writer_write(writer, text, strlen(text));
}
