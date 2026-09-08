#include "test_callable_engine_support.h"

void mmay_test_mapper_value_engines(void) {
    static const char source[] =
        "fn main():int{let calls=0;const input:Result<int,[]int> = Err([3]);"
        "const mapper=(v:[]int):()=>int=>{calls+=1;const _=v.reserve(16);"
        "return ():int=>{v[0]+=1;return v[0]+calls;};};"
        "const mapped=input.mapError(mapper);const ok:Result<void,int> = Ok();"
        "const _=ok.mapError((e:int):void=>{});"
        "return match mapped{Ok(x)=>x,Err(read)=>read()};}";
    mmay_test_callable_engines(source, 5, 3, 3);
}
