#include "../include/tinyhook.h"
#include "private.h"

static int get_jump_size(void *src, void *dst) {
    bool is_far = need_far_jump(src, dst);
#ifdef __arm64__
    return is_far ? 12 : 4;
#elif __x86_64__
    return is_far ? 14 : 5;
#endif
}

int tiny_hook_ex(th_bak_t *bak, void *function, void *destination, void **origin) {
    bak->address = function;
    bak->jump_size = get_jump_size(function, destination);
    read_mem(bak->head_bak, bak->address, bak->jump_size);
    return tiny_hook(function, destination, origin);
}

int tiny_unhook_ex(th_bak_t *bak) {
    return write_mem(bak->address, bak->head_bak, bak->jump_size);
}
