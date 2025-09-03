#ifndef TINYHOOK_PRIVATE_H
#define TINYHOOK_PRIVATE_H

#include <TargetConditionals.h>

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
#else
    #include <printf.h>          // fprintf()
    #define LOG_ERROR(fmt, ...) fprintf(stderr, "ERROR [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define MB (1ll << 20)
#define GB (1ll << 30)

#ifdef __aarch64__
    #define AARCH64_B    0x14000000 // b        +0
    #define AARCH64_BL   0x94000000 // bl       +0
    #define AARCH64_ADRP 0x90000011 // adrp     x17, 0
    #define AARCH64_BR   0xd61f0220 // br       x17
    #define AARCH64_BLR  0xd63f0220 // blr      x17
    #define AARCH64_ADD  0x91000231 // add      x17, x17, 0
    #define AARCH64_SUB  0xd1000231 // sub      x17, x17, 0
#elif __x86_64__
    #define X86_64_CALL     0xe8       // call
    #define X86_64_JMP      0xe9       // jmp
    #define X86_64_JMP_RIP  0x000025ff // jmp      [rip]
    #define X86_64_CALL_RIP 0x000015ff // call     [rip]
    #define X86_64_MOV_RI64 0xb848     // mov      r64, m64
    #define X86_64_MOV_RM64 0x8b48     // mov      r64, [r64]
#endif

bool need_far_jump(const void *src, const void *dst);

#endif
