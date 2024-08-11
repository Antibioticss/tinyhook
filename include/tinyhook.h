#ifndef tinyhook_h
#define tinyhook_h

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int read_mem(void *destnation, const void *source, size_t len);

int write_mem(void *destnation, const void *source, size_t len);

int tiny_hook(void *function, void *destnation, void **origin);

int tiny_insert(void *address, void *destnation, bool link);

int tiny_insert_far(void *address, void *destnation, bool link);

#ifdef __cplusplus
}
#endif

#endif
