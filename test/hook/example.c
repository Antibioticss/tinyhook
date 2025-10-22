#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../include/tinyhook.h"

int (*orig_printf)(const char *format, ...);
int printf_hook(const char *format, ...) {
    // use `orig_printf` to call the original function
    orig_printf("Hooked printf: ");
    va_list args;
    va_start(args, format);
    int res = vprintf(format, args);
    va_end(args);
    return res;
}

int printf_interpose(const char *format, ...) {
    // use `printf` directly to call the original function
    printf("Interposed printf: ");
    va_list args;
    va_start(args, format);
    int res = vprintf(format, args);
    va_end(args);
    return res;
}

int fake_add(int a, int b) {
    fprintf(stderr, "=== calling fake_add with: %d, %d\n", a, b);
    return -1;
}

__attribute__((visibility("default"))) void exported_func() {
    printf("This is an exported function\n");
    return;
}

__attribute__((constructor(0))) int load() {
    fprintf(stderr, "=== libexample loading...\n");

    // get an exported symbol address
    void (*func_addr)(void) = symexp_solve(1, "_exported_func");
    fprintf(stderr, "=== exported_func() address: %p\n", func_addr);
    func_addr();

    // hook a function by symbol (in the SYMTAB)
    void *func_add = symtbl_solve(0, "_add");
    fprintf(stderr, "=== add() address: %p\n", func_add);
    tiny_hook(func_add, fake_add, NULL);

    // hook system function
    fprintf(stderr, "=== Hooking printf\n");
    th_bak_t printf_bak;
    tiny_hook_ex(&printf_bak, printf, printf_hook, (void **)&orig_printf);
    printf("Hello, world!\n");
    // remove hook
    fprintf(stderr, "=== Removing hook\n");
    tiny_unhook_ex(&printf_bak);
    printf("Hook is removed!\n");

    // or use interpose to hook it!
    fprintf(stderr, "=== Now interposing printf\n");
    tiny_interpose(0, "_printf", printf_interpose);
    return 0;
}
