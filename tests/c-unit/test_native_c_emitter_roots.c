#include "test_callable_engine_support.h"

void mmay_test_native_c_emitter_roots(void) {
    static const char source[] =
        "fn pressure():void{const make=():[]int=>[1];const _=make();}"
        "fn join(left:bool):int{const value=match left{true=>[5],false=>[7]};"
        "pressure();return value[0];}"
        "fn rebind():int{let value=[9];const saved=value;value=[2];"
        "pressure();return saved[0];}"
        "fn loop():int{let value=[1];let saved=value;for i in 0..3{"
        "saved=value;value=[i+2];pressure();}return saved[0];}"
        "type Bag{mutable value:[]int,}"
        "fn replace(bag:Bag):void{bag.value=[8];}"
        "fn child():int{const bag=Bag{value:[7]};const saved=bag.value;"
        "replace(bag);pressure();return saved[0];}"
        "fn replaceItem(values:[][]int):void{values[0]=[8];}"
        "fn item():int{const values=[[6]];const saved=values[0];"
        "replaceItem(values);pressure();return saved[0];}"
        "fn main():int{return join(true)+join(false)+rebind()+loop()+child()+item();}";
    mmay_test_callable_engines(source, 37, 1, 1);
}
