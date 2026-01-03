#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>

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

int (*orig_add)(int a, int b);
int fake_add(int a, int b) {
    fprintf(stderr, "=== calling add with: %d, %d\n", a, b);
    fprintf(stderr, "=== the return value is: %d\n", orig_add(a, b));
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
    tiny_hook(func_add, fake_add, &orig_add);

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

static void *(*orig_dlopen)(const char *filename, int flag);
void *my_dlopen(const char *filename, int flag) {
    printf("\033[1;31m[dlopen]\033[1;33m filename:\033[0m %s\033[1;33m flag:\033[0m 0x%x\n", filename, flag);
    void *ret = orig_dlopen(filename, flag);
    printf("\033[1;32m[>]\033[0m handler: %p\n", ret);
    return ret;
}

int (*orig_sysctl)(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen);
int my_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen) {
    int ret = orig_sysctl(name, namelen, oldp, oldlenp, newp, newlen);
    fprintf(stderr, "=== sysctl hooked! Return value: %d\n", ret);
    return ret;
}

void get_cpu_cores(void) {
    int mib[2];
    int cpu_count;
    size_t len = sizeof(cpu_count);

    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    sysctl(mib, 2, &cpu_count, &len, NULL, 0);

    printf("CPU cores: %d\n", cpu_count);
    return;
}

__attribute__((constructor(1))) int load2() {
    tiny_hook(dlopen, my_dlopen, (void **)&orig_dlopen); // test adrp for arm64
    void *handle = dlopen("./libexample.dylib", RTLD_LAZY);
    dlclose(handle);
    tiny_hook(sysctl, my_sysctl, (void **)&orig_sysctl); // test jne for intel
    get_cpu_cores();
    return 0;
}
