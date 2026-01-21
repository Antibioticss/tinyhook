#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>

#include "tinyhook.h"

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
    (void)fprintf(stderr, "=== calling add with: %d, %d\n", a, b);
    (void)fprintf(stderr, "=== the return value is: %d\n", orig_add(a, b));
    return -1;
}

__attribute__((visibility("default"))) void exported_func() {
    printf("This is an exported function\n");
    return;
}

__attribute__((constructor(0))) int load() {
    (void)fprintf(stderr, "=== libexample loading...\n");

    // get an exported symbol address
    void (*func_addr)(void) = symexp_solve(1, "_exported_func");
    (void)fprintf(stderr, "=== exported_func() address: %p\n", func_addr);
    func_addr();

    // hook a function by symbol (in the SYMTAB)
    void *func_add = symtbl_solve(0, "_add");
    (void)fprintf(stderr, "=== add() address: %p\n", func_add);
    tiny_hook(func_add, fake_add, (void **)&orig_add);

    // hook system function
    (void)fprintf(stderr, "=== Hooking printf\n");
    th_bak_t printf_bak;
    tiny_hook_ex(&printf_bak, printf, printf_hook, (void **)&orig_printf);
    printf("Hello, world!\n");
    // remove hook
    (void)fprintf(stderr, "=== Removing hook\n");
    tiny_unhook_ex(&printf_bak);
    printf("Hook is removed!\n");

    // or use interpose to hook it!
    (void)fprintf(stderr, "=== Now interposing printf\n");
    tiny_interpose(0, "_printf", printf_interpose, NULL);
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
    (void)fprintf(stderr, "=== sysctl hooked! Return value: %d\n", ret);
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

__attribute__((constructor(1))) void load1() {
    tiny_hook(dlopen, my_dlopen, (void **)&orig_dlopen); // test adrp for arm64
    void *handle = dlopen("./libexample.dylib", RTLD_LAZY);
    dlclose(handle);
    tiny_hook(sysctl, my_sysctl, (void **)&orig_sysctl); // test jne for intel
    get_cpu_cores();
    return;
}

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int (*orig_connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void)fprintf(stderr, "hooked connect\n");
    return orig_connect(sockfd, addr, addrlen);
}

int (*orig_socket)(int domain, int type, int protocol);
int my_socket(int domain, int type, int protocol) {
    (void)fprintf(stderr, "hooked socket\n");
    return orig_socket(domain, type, protocol);
}

int (*orig_close)(int fd);
int my_close(int fd) {
    (void)fprintf(stderr, "hooked close\n");
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
    return;
}

// todo : add support for syscalls, e.g. open, close, read
/*
 * _socket:
 mov        x16, #0x61
 svc        #0x80
 b.lo       loc_180435e80 <---- add handle for this!

 libsystem_kernel.dylib`socket:
     0x7ff80a57fe5c <+0>:  mov    eax, 0x2000061
     0x7ff80a57fe61 <+5>:  mov    r10, rcx
     0x7ff80a57fe64 <+8>:  syscall
     0x7ff80a57fe66 <+10>: jae    0x7ff80a57fe70 ; <+20>
     0x7ff80a57fe68 <+12>: mov    rdi, rax
     0x7ff80a57fe6b <+15>: jmp    0x7ff80a57d29a ; cerror_nocancel
     0x7ff80a57fe70 <+20>: ret
 */
