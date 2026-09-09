#include "test_callable_engine_support.h"

/* Array argument buffers remain owned across literal-string promotion, whose
 * allocation failure branches directly to the function cleanup label. */
void mmay_test_native_c_emitter_array_cells(void) {
    static const char source[] =
        "fn main():int{const empty:[]string=[];const a=[\"a\",\"bc\"];"
        "const nested=[a,[\"def\"]];const unused=[\"unused\"];"
        "if nested[0][1]==\"bc\"&&nested[1][0]==\"def\"{"
        "return nested.length()+nested[0].length()+empty.length();}return 0;}";
    mmay_test_callable_engines(source, 4, 0, 0);
}
