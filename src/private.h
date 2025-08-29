#ifndef TINYHOOK_PRIVATE_H
#define TINYHOOK_PRIVATE_H

#ifndef COMPACT
#include <mach/mach_error.h> // mach_error_string()
#include <printf.h>          // fprintf()
#define LOG_ERROR(fmt, ...) fprintf(stderr, "ERROR [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...) ((void)0)
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

bool need_far_jump(void *src, void *dst);

#endif
