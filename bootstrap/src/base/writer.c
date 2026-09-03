#include "mmay/base/writer.h"

#include <string.h>

static bool mmay_file_write(void *context, const char *bytes, size_t length) {
    FILE *file = context;
    if (file == NULL) {
        return false;
    }
    return fwrite(bytes, 1, length, file) == length;
}

static bool mmay_file_flush(void *context) {
    FILE *file = context;
    return file != NULL && fflush(file) == 0;
}

MarimayWriter mmay_file_writer(FILE *file) {
    MarimayWriter writer;
    writer.context = file;
    writer.write = mmay_file_write;
    writer.flush = mmay_file_flush;
    return writer;
}

FILE *mmay_writer_file(MarimayWriter writer) {
    return writer.write == mmay_file_write && writer.flush == mmay_file_flush
        ? writer.context : NULL;
}

bool mmay_writer_write(MarimayWriter writer, const char *bytes, size_t length) {
    if (writer.write == NULL || (bytes == NULL && length != 0)) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    return writer.write(writer.context, bytes, length);
}

bool mmay_writer_write_cstr(MarimayWriter writer, const char *text) {
    if (text == NULL) {
        return false;
    }
    return mmay_writer_write(writer, text, strlen(text));
}

bool mmay_writer_flush(MarimayWriter writer) {
    return writer.flush == NULL || writer.flush(writer.context);
}
