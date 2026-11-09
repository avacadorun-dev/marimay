/* POSIX stdio owner: streams already transport exact bytes. */
#include "mmay/platform/stdio.h"

bool mmay_platform_stdio_binary(FILE *stream) {
    return stream != NULL;
}
