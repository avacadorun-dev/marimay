#ifndef MMAY_PLATFORM_STDIO_H
#define MMAY_PLATFORM_STDIO_H

#include <stdbool.h>
#include <stdio.h>

/*
 * Disables host text translation for a borrowed open stream. The caller keeps
 * ownership and must call this before any I/O; no storage is allocated. POSIX
 * streams already preserve bytes. Windows uses the CRT binary descriptor mode.
 */
bool mmay_platform_stdio_binary(FILE *stream);

#endif
