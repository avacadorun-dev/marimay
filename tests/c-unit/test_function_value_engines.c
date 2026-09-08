#include "test_callable_engine_support.h"

static const char source[] =
    "fn unused(f:(bool)=>string):string{return f(true); }\n"
    "fn factory():()=>[]()=>int {let n=1;return ():[]()=>int=>[\n"
    " ():int=>{n+=1;return n;},():int=>n];}\n"
    "fn main():int {const a=factory()();const x=a[0]();const y=a[1]();\n"
    " const step=():void=>{const _=a[0]();};step();\n"
    " let f:()=>int=():int=>0;const cycle=():int=>{const _=f;return 0;};f=cycle;\n"
    " return x*100+y*10+a[1]()+f();}\n";


void mmay_test_function_value_engines(void) {
    mmay_test_callable_engines(source, 223, 5, 5);
}
