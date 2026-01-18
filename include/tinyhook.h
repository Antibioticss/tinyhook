#ifndef tinyhook_h
#define tinyhook_h

#include <objc/runtime.h>
#include <stdint.h>

#ifdef NO_EXPORT
#define TH_API __attribute__((visibility("hidden")))
#else
#define TH_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *address;
    int jump_size;
    uint8_t head_bak[16];
} th_bak_t;

/* inline hook */
TH_API int tiny_hook(void *function, void *destination, void **origin);

TH_API int tiny_hook_ex(th_bak_t *bak, void *function, void *destination, void **origin);

TH_API int tiny_unhook_ex(const th_bak_t *bak);

TH_API int tiny_insert(void *address, void *destination);

/* interpose */
TH_API int tiny_interpose(uint32_t image_index, const char *symbol_name, void *replacement);

/* objective-c runtime */
TH_API int ocrt_hook(const char *cls, const char *sel, void *destination, void **origin);

TH_API int ocrt_swap(const char *cls1, const char *sel1, const char *cls2, const char *sel2);

TH_API void *ocrt_impl(char type, const char *cls, const char *sel);

TH_API Method ocrt_method(char type, const char *cls, const char *sel);

/* memory access */
TH_API int read_mem(void *destination, const void *source, size_t len);

TH_API int write_mem(void *destination, const void *source, size_t len);

/* symbol resolve */
TH_API void *symtbl_solve(uint32_t image_index, const char *symbol_name);

TH_API void *symexp_solve(uint32_t image_index, const char *symbol_name);

#ifdef __cplusplus
}
#endif

#endif
