#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>

#include "tinyhook.h"

int (*orig_printf)(const char *format, ...);
int printf_hooked(const char *format, ...) {
    // use `orig_printf` to call the original function
    orig_printf("hooked printf: ");
    va_list args;
    va_start(args, format);
    int res = vprintf(format, args);
    va_end(args);
    return res;
}

int printf_interposed(const char *format, ...) {
    // use `printf` directly to call the original function
    printf("interposed printf: ");
    va_list args;
    va_start(args, format);
    int res = vprintf(format, args);
    va_end(args);
    return res;
}

int (*orig_add)(int a, int b);
int fake_add(int a, int b) {
    printf("[=] add() hooked! args: %d, %d\n", a, b);
    int ret = orig_add(a, b);
    printf("[>] returned: %d, changing to -1...\n", ret);
    ret = -1;
    return ret;
}

__attribute__((visibility("default"))) void exported_func() {
    printf("An exported function in libexample\n");
    return;
}

__attribute__((constructor(0))) int load0() {
    printf("[=] libexample loading...\n");

    // resolve exported symbol address
    void (*func_addr)(void) = symbol_resolve(1, "_exported_func", RESOLVE_EXPORT);
    printf("=== exported_func() in libexample address: %p\n", func_addr);
    func_addr();

    // resolve symbol table symbol address
    void *func_add = symbol_resolve(0, "_add", RESOLVE_SYMTAB);
    printf("=== add() in main address: %p\n", func_add);

    // resolve symbol stub address
    size_t (*stub_strlen)(const char *str) = symbol_resolve(0, "_strlen", RESOLVE_STUBS);
    printf("=== strlen() stub in main address: %p\n", stub_strlen);
    printf("Calling stub: strlen('tinyhook') = %lu\n", stub_strlen("tinyhook"));

    // hook a simple function
    tiny_hook(func_add, fake_add, (void **)&orig_add);

    // hook system function
    printf("=== Hooking printf\n");
    th_bak_t printf_bak;
    tiny_hook_ex(&printf_bak, printf, printf_hooked, (void **)&orig_printf);
    printf("Hello, world!\n");
    // remove hook
    printf("=== Removing hook on printf\n");
    tiny_unhook_ex(&printf_bak);
    printf("Hook is removed!\n");

    // or use interpose to hook it!
    printf("=== Now interpose printf\n");
    tiny_interpose(0, "_printf", printf_interposed, NULL);
    return 0;
}

static void *(*orig_dlopen)(const char *filename, int flag);
void *my_dlopen(const char *filename, int flag) {
    printf("[=] dlopen() hooked! args: %s, 0x%x\n", filename, flag);
    void *ret = orig_dlopen(filename, flag);
    printf("[>] returned handler: %p\n", ret);
    return ret;
}

int (*orig_sysctl)(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen);
int my_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen) {
    printf("[=] sysctl() hooked!\n");
    int ret = orig_sysctl(name, namelen, oldp, oldlenp, newp, newlen);
    printf("[>] returned: %d\n", ret);
    return ret;
}

void test_dlopen(void) {
    void *handle = dlopen("./libexample.dylib", RTLD_LAZY);
    dlclose(handle);
}

void test_sysctl(void) {
    int mib[2];
    int cpu_count;
    size_t len = sizeof(cpu_count);

    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    sysctl(mib, 2, &cpu_count, &len, NULL, 0);
}

__attribute__((constructor(1))) void load1() {
    tiny_hook(dlopen, my_dlopen, (void **)&orig_dlopen); // test adrp for arm64
    tiny_hook(sysctl, my_sysctl, (void **)&orig_sysctl); // test jne for intel

    test_dlopen();
    test_sysctl();
}

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int (*orig_connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    printf("[=] connect() hooked!\n");
    return orig_connect(sockfd, addr, addrlen);
}

int (*orig_socket)(int domain, int type, int protocol);
int my_socket(int domain, int type, int protocol) {
    printf("[=] socket() hooked!\n");
    return orig_socket(domain, type, protocol);
}

int (*orig_close)(int fd);
int my_close(int fd) {
    printf("[=] close() hooked!\n");
    return orig_close(fd);
}

__attribute__((constructor(2))) void load2() {
    // test hooks on syscall functions
    tiny_hook(socket, my_socket, (void **)&orig_socket);
    tiny_hook(connect, my_connect, (void **)&orig_connect);
    tiny_hook(close, my_close, (void **)&orig_close);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {AF_INET, (sa_family_t)htons(1234), inet_addr("1.2.3.4")};
    if (connect(fd, (struct sockaddr *)&a, sizeof(a))) perror("connect");
    close(fd);
}
