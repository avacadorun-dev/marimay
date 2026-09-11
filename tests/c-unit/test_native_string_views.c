#include "test_callable_engine_support.h"

/* Real source, both engines, forced collection and every allocation failure in
 * lowering, encoding, execution and generated C at both -O0 and -O2. */
void mmay_test_native_string_views(void) {
    static const char source[] =
        "fn pressure():void{const temporary=[\"garbage\"];const _=temporary.length();}\n"
        "fn changed(s:string):string{return $\"a\\0{s}\";}\n"
        "fn main():int{let values=[\"\",\"a\\0b\",\"ж\"];let score=0;"
        "const alias=values;const old=values[1];"
        "const replacement=changed(\"c\");values[1]=replacement;pressure();"
        "if old==\"a\\0b\"&&alias[1]==\"a\\0c\"{score+=1;}"
        "for index in 0..3{const selected=values[index];pressure();"
        "if selected==values[index]{score+=1;}}"
        "values=[\"new\"];pressure();"
        "if alias[0]==\"\"&&alias[2]==\"ж\"&&values[0]==\"new\"{score+=1;}"
        "return score;}\n";
    mmay_test_callable_engines(source,5,0,0);
}
