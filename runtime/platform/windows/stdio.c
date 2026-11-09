/* Windows stdio owner: remove CRT CRLF and control-Z text translation. */
#include "mmay/platform/stdio.h"

#include <fcntl.h>
#include <io.h>

bool mmay_platform_stdio_binary(FILE *stream) {
    int descriptor;
    if (stream == NULL) return false;
    descriptor = _fileno(stream);
    return descriptor >= 0 && _setmode(descriptor, _O_BINARY) != -1;
}
