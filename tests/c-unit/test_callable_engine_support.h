#ifndef MMAY_TEST_CALLABLE_ENGINE_SUPPORT_H
#define MMAY_TEST_CALLABLE_ENGINE_SUPPORT_H
#include <stddef.h>
#include <stdint.h>

/* Shared D098/D099 execution, wire, GC and allocation-failure harness. */
void mmay_test_callable_engines(const char *source, int64_t expected,
    size_t minimum_calls, size_t minimum_captures);
#endif
