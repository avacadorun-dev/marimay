#include "test_callable_engine_support.h"

/* Cached native pointers must follow the current managed reference on every
 * loop iteration, including immutable child hints passed to a direct callee. */
void mmay_test_native_c_emitter_view_lifetimes(void) {
    static const char source[] =
        "type Bag { values: []int, }\n"
        "type Scalar { value: int, }\n"
        "fn first(bag: Bag): int { return bag.values[0]; }\n"
        "fn pressure(): void { const make = (): []int => [1]; const _ = make(); }\n"
        "fn local(): int { let sum = 0; for i in 1..4 {\n"
        "  const bag = Bag { values: [i] };\n"
        "  sum += bag.values[0]; sum += first(bag); pressure();\n"
        "} return sum; }\n"
        "fn selected(): int { let sum = 0; const bags = [\n"
        "  Bag { values: [1] }, Bag { values: [2] }, Bag { values: [3] }];\n"
        "  for bag in bags { const values = bag.values; sum += values[0] + first(bag); pressure(); }\n"
        "  return sum; }\n"
        "fn scalar(): int { let sum = 0; for i in 1..4 {\n"
        "  const item = Scalar { value: i }; sum += item.value; pressure();\n"
        "} return sum; }\n"
        "fn rebind(): int { let item = Scalar { value: 0 }; let sum = 0;\n"
        "for i in 1..4 { item = Scalar { value: i }; sum += item.value; pressure(); } return sum; }\n"
        "fn main(): int { return local() + selected() + scalar() + rebind(); }\n";
    mmay_test_callable_engines(source, 36, 1, 1);
}
