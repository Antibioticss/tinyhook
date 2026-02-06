#include "private.h"
#include "tinyhook.h"

static int get_jump_size(const void *src, const void *dst) {
    bool is_far = need_far_jump(src, dst);
#ifdef __arm64__
    return is_far ? 12 : 4;
#elif __x86_64__
    return is_far ? 14 : 5;
#endif
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
    ARG_CHECK(bak != 0);
    return write_mem(bak->address, bak->head_bak, bak->jump_size);
}
