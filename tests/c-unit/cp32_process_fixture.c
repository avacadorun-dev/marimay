#include <stdio.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

int main(void) {
    unsigned char bytes[4096];
    size_t count = 0;
#if defined(_WIN32)
    if (_setmode(_fileno(stdin), _O_BINARY) == -1
        || _setmode(_fileno(stdout), _O_BINARY) == -1
        || _setmode(_fileno(stderr), _O_BINARY) == -1) return 2;
#endif
    while ((count = fread(bytes, 1, sizeof(bytes), stdin)) != 0) {
        if (fwrite(bytes, 1, count, stdout) != count) return 3;
    }
    if (ferror(stdin)) return 4;
    if (fwrite("fixture-error\n", 1, sizeof("fixture-error\n") - 1, stderr)
            != sizeof("fixture-error\n") - 1
        || fflush(stdout) != 0 || fflush(stderr) != 0) return 5;
    return 0;
}
