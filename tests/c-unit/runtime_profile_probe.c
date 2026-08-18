#include <stdio.h>

#include "ruda/runtime/profile.h"

int main(void) {
    return puts(ruda_runtime_build_profile()) < 0 ? 1 : 0;
}
