#ifndef MMAY_CHECKER_ENUM_BOTTOM_API_H
#define MMAY_CHECKER_ENUM_BOTTOM_API_H
#include "checker_function_context.h"

/* A direct never payload makes a concrete variant impossible. */
bool mmay_enum_variant_bottom(MarimayFunctionChecker *function,
    const MarimayCheckEnumDeclaration *declaration, MarimayTypeId type,
    const MarimayCheckEnumVariantDeclaration *variant, bool *bottom);
bool mmay_enum_bottom_coverage(MarimayFunctionChecker *function,
    const MarimayCheckEnumDeclaration *declaration, MarimayTypeId type,
    unsigned char *seen, bool *all_bottom);
/* No source value is fabricated on a statically impossible enum edge. */
bool mmay_enum_bottom_edge(MarimayFunctionChecker *function,
    size_t start, size_t end, MarimayExpressionValue *result);
#endif
