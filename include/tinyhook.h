#ifndef tinyhook_h
#define tinyhook_h

#include <objc/objc-runtime.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* inline hook */
int tiny_hook(void *function, void *destination, void **origin);

int tiny_insert(void *address, void *destination, bool link);

int tiny_insert_far(void *address, void *destination, bool link);

/* objective-c runtime */
int ocrt_hook(const char *cls, const char *sel, void *destination, void **origin);

int ocrt_swap(const char *cls1, const char *sel1, const char *cls2, const char *sel2);

void *ocrt_impl(char type, const char *cls, const char *sel);

Method ocrt_method(char type, const char *cls, const char *sel);

/* memory access */
int read_mem(void *destination, const void *source, size_t len);

int write_mem(void *destination, const void *source, size_t len);

/* solve symbol */
void *symtbl_solve(uint32_t image_index, const char *symbol_name);

void *symexp_solve(uint32_t image_index, const char *symbol_name);

#ifdef __cplusplus
}
#endif

#endif
