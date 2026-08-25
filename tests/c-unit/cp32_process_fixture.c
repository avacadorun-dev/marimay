#include <stdio.h>

int main(void) {
    unsigned char bytes[4096];
    size_t count = 0;
    while ((count = fread(bytes, 1, sizeof(bytes), stdin)) != 0) {
        if (fwrite(bytes, 1, count, stdout) != count) return 3;
    }
    if (ferror(stdin)) return 4;
    if (fwrite("fixture-error\n", 1, sizeof("fixture-error\n") - 1, stderr)
            != sizeof("fixture-error\n") - 1
        || fflush(stdout) != 0 || fflush(stderr) != 0) return 5;
    return 0;
}
