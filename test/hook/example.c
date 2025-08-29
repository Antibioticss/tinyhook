#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../include/tinyhook.h"

int (*orig_printf)(const char *format, ...);
int my_printf(const char *format, ...) {
    char prefix[] = "Hooked printf: ";
    size_t new_fmt_len = strlen(format) + strlen(prefix);
    char *new_format = malloc(new_fmt_len + 1);
    strcpy(new_format, prefix);
    strcat(new_format, format);
    va_list args;
    va_start(args, format);
    int res = vprintf(new_format, args);
    va_end(args);
    free(new_format);
    return res;
}

int fake_add(int a, int b) {
    fprintf(stderr, "=== calling fake_add with: %d, %d\n", a, b);
    return -1;
}

__attribute__((visibility("default"))) void exported_func() {
    return;
}

__attribute__((constructor(0))) int load() {
    fprintf(stderr, "=== libexample loading...\n");

    // get an exported symbol address
    void *func_fake = symexp_solve(1, "_exported_func");
    fprintf(stderr, "=== exported_func() address: %p\n", func_fake);

    // hook a function by symbol
    void *func_add = symtbl_solve(0, "_add");
    fprintf(stderr, "=== add() address: %p\n", func_add);
    tiny_hook(func_add, fake_add, NULL);

    // hook system function
    fprintf(stderr, "=== Hooking printf\n");
    th_bak_t printf_bak;
    tiny_hook_ex(&printf_bak, printf, my_printf, (void **)&orig_printf);
    printf("Hello, world!\n");
    // remove hook
    fprintf(stderr, "=== Removing hook\n");
    tiny_unhook_ex(&printf_bak);
    printf("Hook is removed!\n");
    return 0;
}
