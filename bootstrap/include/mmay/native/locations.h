#ifndef MMAY_NATIVE_LOCATIONS_H
#define MMAY_NATIVE_LOCATIONS_H

#include "mmay/types/type.h"

/* Optional immutable debug metadata, sorted uniquely by symbol/start/end.
 * Path bytes are borrowed through emission, counted UTF-8 without NUL.
 * Coordinates are already resolved by the source owner under D022. */
typedef struct MarimayNativePanicLocation {
    MarimaySymbolId symbol;
    size_t start, end;
    const char *path;
    size_t path_length;
    size_t line, column, end_line, end_column;
} MarimayNativePanicLocation;

#endif
