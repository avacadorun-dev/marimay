#include "test_callable_engine_support.h"

void mmay_test_array_search_engines(void) {
    static const char source[] =
        "fn main():int{const a=[[1],[2],[3]];let calls=0;\n"
        " const predicate=(x:[]int):bool=>{calls+=1;const _=a.reserve(16);\n"
        "  const child=x.copy();return child[0]==2;};\n"
        " const hit=a.findIndex(predicate);const empty:[][]int=[];\n"
        " const absent=a.findIndex((x:[]int):bool=>false);\n"
        " const none=empty.findIndex(predicate);\n"
        " return match hit{Some(i)=>i*100+calls,None=>0};}\n";
    mmay_test_callable_engines(source, 102, 3, 2);
}
