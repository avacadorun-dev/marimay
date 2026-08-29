#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int descriptors[2];
    if (argc < 2) {
        fputs("usage: cp32_broken_pipe_launcher COMMAND [ARG...]\n", stderr);
        return 126;
    }
    if (pipe(descriptors) != 0) {
        perror("pipe");
        return 126;
    }
    if (close(descriptors[0]) != 0) {
        perror("close");
        (void)close(descriptors[1]);
        return 126;
    }
    if (dup2(descriptors[1], STDOUT_FILENO) < 0) {
        perror("dup2");
        (void)close(descriptors[1]);
        return 126;
    }
    if (descriptors[1] != STDOUT_FILENO) {
        (void)close(descriptors[1]);
    }
    execv(argv[1], &argv[1]);
    perror("execv");
    return 126;
}
