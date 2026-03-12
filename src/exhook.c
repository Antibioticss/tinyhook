#include "private.h"
#include "tinyhook.h"

static int get_jump_size(void *src, void *dst) {
    int jump_size = 0;
    int64_t gap = (int64_t)dst - (int64_t)src;
#ifdef __aarch64__
    if (gap <= 0x8000000 - 1 && gap >= -0x8000000)
        jump_size = 4;
    else {
        gap = ((int64_t)dst >> 12) - ((int64_t)src >> 12);
        if (gap <= 0x100000 - 1 && gap >= -0x100000)
            jump_size = 12;
        else {
            jump_size = 4;
            uint64_t imm64 = (uint64_t)dst;
            for (int i = 0; imm64; imm64 >>= 16, i++)
                if (imm64 & 0xffff) jump_size += 4;
        }
    }
#elif __x86_64__
    if (gap <= INT32_MAX && gap >= INT32_MIN)
        jump_size = 5;
    else
        jump_size = 14;
#endif
    return jump_size;
}

int tiny_hook_ex(th_bak_t *bak, void *function, void *destination, void **origin) {
    ARG_CHECK(bak != NULL);
    ARG_CHECK(function != NULL);
    ARG_CHECK(destination != NULL);
    bak->address = function;
    bak->jump_size = get_jump_size(function, destination);
    read_mem(bak->head_bak, bak->address, bak->jump_size);
    return tiny_hook(function, destination, origin);
}

int tiny_unhook_ex(const th_bak_t *bak) {
    ARG_CHECK(bak != NULL);
    return write_mem(bak->address, bak->head_bak, bak->jump_size);
}
