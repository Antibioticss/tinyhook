#ifndef TINYHOOK_PRIVATE_H
#define TINYHOOK_PRIVATE_H

#include <TargetConditionals.h>
#include <stdlib.h>

#ifdef __aarch64__
    #define MAX_JUMP_SIZE 20
    #define MAX_HEAD_SIZE 48 // size of saved function header
#elif __x86_64__
    #define MAX_JUMP_SIZE 14
    #define MAX_HEAD_SIZE 35
#endif

#include <mach/mach_init.h> // mach_task_self()
#if TARGET_OS_OSX
    #include <mach/mach_vm.h> // mach_vm_*
#elif TARGET_OS_IOS
    #include <mach/vm_map.h> // vm_*
    #define mach_vm_address_t  vm_address_t
    #define mach_vm_allocate   vm_allocate
    #define mach_vm_deallocate vm_deallocate
    #define mach_vm_read       vm_read
    #define mach_vm_write      vm_write
    #define mach_vm_protect    vm_protect
#endif

#ifdef COMPACT
    #define LOG_ERROR(fmt, ...) ((void)0)
    #define ARG_CHECK(x)        ((void)0)
#else
    #if TARGET_OS_OSX
        #include <printf.h> // fprintf()
        #define LOG_ERROR(fmt, ...) (void)fprintf(stderr, "ERROR [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
    #elif TARGET_OS_IOS
        #include <os/log.h> // os_log()
        #define LOG_ERROR(fmt, ...)                                                                                    \
            os_log_with_type(OS_LOG_DEFAULT, OS_LOG_TYPE_ERROR, "ERROR [%s:%d]: " fmt "\n", __FILE__, __LINE__,        \
                             ##__VA_ARGS__)
    #endif
    #define ARG_CHECK(x)                                                                                               \
        if (!(x)) {                                                                                                    \
            LOG_ERROR("ARG_CHECK '%s' failed", #x);                                                                    \
            abort();                                                                                                   \
        }
#endif

#endif
