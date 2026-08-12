#ifndef RUDA_BUILD_CONFIG_H
#define RUDA_BUILD_CONFIG_H

/* Fallback for direct C17 bootstrap builds; CMake supplies a generated header. */
#define RUDA_COMPILER_VERSION "0.0.0-dev"
#define RUDA_RUNTIME_VERSION "0.0.0-dev"
#define RUDA_LANGUAGE_VERSION "0.1-design"
#define RUDA_BYTECODE_VERSION "unavailable"
#define RUDA_STANDARD_LIBRARY_VERSION "unavailable"
#define RUDA_BOOTSTRAP_STAGE 0

#if defined(_WIN32)
#define RUDA_HOST_SYSTEM "Windows"
#elif defined(__APPLE__)
#define RUDA_HOST_SYSTEM "Darwin"
#elif defined(__linux__)
#define RUDA_HOST_SYSTEM "Linux"
#else
#define RUDA_HOST_SYSTEM "Unknown"
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
#define RUDA_HOST_PROCESSOR "arm64"
#elif defined(_M_X64) || defined(__x86_64__)
#define RUDA_HOST_PROCESSOR "x86_64"
#elif defined(_M_IX86) || defined(__i386__)
#define RUDA_HOST_PROCESSOR "x86"
#else
#define RUDA_HOST_PROCESSOR "unknown"
#endif

#endif
