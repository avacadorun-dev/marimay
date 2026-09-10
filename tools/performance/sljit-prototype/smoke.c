#include "sljitLir.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct Input { int64_t left, right, result; uintptr_t status; } Input;
typedef void (SLJIT_FUNC *Entry)(Input *);
int main(void) {
    struct sljit_compiler *c = sljit_create_compiler(NULL);
    if (c == NULL) return 2;
    sljit_emit_enter(c, 0, SLJIT_ARGS1V(P), 2, 1, 0);
    sljit_emit_op1(c, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Input,left));
    sljit_emit_op1(c, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Input,right));
    sljit_emit_op2(c, SLJIT_ADD | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
    struct sljit_jump *overflow = sljit_emit_jump(c, SLJIT_OVERFLOW);
    sljit_emit_op1(c, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), offsetof(Input,result), SLJIT_R0, 0);
    sljit_emit_op1(c, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), offsetof(Input,status), SLJIT_IMM, 0);
    struct sljit_jump *done = sljit_emit_jump(c, SLJIT_JUMP);
    sljit_set_label(overflow, sljit_emit_label(c));
    sljit_emit_op1(c, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), offsetof(Input,status), SLJIT_IMM, 1);
    sljit_set_label(done, sljit_emit_label(c));
    sljit_emit_return_void(c);
    void *code = sljit_generate_code(c, 0, NULL);
    if (code == NULL) { sljit_free_compiler(c); return 3; }
    Entry entry = NULL;
    _Static_assert(sizeof(entry)==sizeof(code), "code pointer size");
    memcpy(&entry,&code,sizeof(entry));
    Input values[]={{42,1,777,99},{-9,4,777,99},{INT64_MAX,1,777,99},{INT64_MIN,-1,777,99}};
    const int64_t results[]={43,-5,777,777};
    int failed=0;
    for (size_t i=0;i<4;i++) { entry(&values[i]); if(values[i].result!=results[i] || values[i].status!=(i>=2)) failed=1; }
    printf("%s: checked arithmetic, branches, memory, unchanged result on overflow: %s (%lu code bytes)\n", sljit_get_platform_name(), failed?"FAILED":"passed",(unsigned long)sljit_get_generated_code_size(c));
    sljit_free_compiler(c);
    sljit_free_code(code,NULL);
    return failed;
}
