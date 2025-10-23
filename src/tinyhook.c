#include "../include/tinyhook.h"
#include "private.h"

#include <stdint.h>
#include <string.h> // memcpy()
#ifndef COMPACT
    #include <mach/mach_error.h> // mach_error_string()
#endif

#ifdef __x86_64__
    #include "fde64/fde64.h"
#endif

bool need_far_jump(const void *src, const void *dst) {
    long long distance = dst > src ? dst - src : src - dst;
#ifdef __aarch64__
    return distance >= 128 * MB;
#elif __x86_64__
    return distance >= 2 * GB;
#endif
}

static int calc_near_jump(uint8_t *output, void *src, void *dst, bool link) {
    int jump_size;
#ifdef __aarch64__
    // b/bl imm    ; go to dst
    jump_size = 4;
    uint32_t insn = (dst - src) >> 2 & 0x3ffffff;
    insn |= link ? AARCH64_BL : AARCH64_B;
    *(uint32_t *)output = insn;
#elif __x86_64__
    // jmp/call imm    ; go to dst
    jump_size = 5;
    *output = link ? X86_64_CALL : X86_64_JMP;
    *(int32_t *)(output + 1) = (int64_t)dst - (int64_t)src - 5;
#endif
    return jump_size;
}

static int calc_far_jump(uint8_t *output, void *src, void *dst, bool link) {
    int jump_size;
#ifdef __aarch64__
    // adrp    x17, imm
    // add     x17, x17, imm    ; x17 -> dst
    // br/blr  x17
    jump_size = 12;
    int64_t insn = ((int64_t)dst >> 12) - ((int64_t)src >> 12);
    insn = ((insn & 0x3) << 29) | ((insn & 0x1ffffc) << 3) | AARCH64_ADRP;
    *(uint32_t *)output = insn;
    insn = ((int64_t)dst & 0xfff) << 10 | AARCH64_ADD;
    *(uint32_t *)(output + 4) = insn;
    *(uint32_t *)(output + 8) = link ? AARCH64_BLR : AARCH64_BR;
#elif __x86_64__
    jump_size = 14;
    // jmp [rip]    ; rip stored dst
    *(uint32_t *)output = link ? X86_64_CALL_RIP : X86_64_JMP_RIP;
    *(uint16_t *)(output + 4) = 0;
    *(uint64_t *)(output + 6) = (uint64_t)dst;
#endif
    return jump_size;
}

static int calc_jump(uint8_t *output, void *src, void *dst, bool link) {
    if (need_far_jump(src, dst))
        return calc_far_jump(output, src, dst, link);
    else
        return calc_near_jump(output, src, dst, link);
}

static void *trampo;
static mach_vm_address_t vmbase;

static inline void save_header(void **src, void **dst, int min_len) {
    mach_vm_protect(mach_task_self(), vmbase, PAGE_SIZE, FALSE, VM_PROT_DEFAULT);
#ifdef __aarch64__
    uint32_t insn;
    for (int i = 0; i < min_len; i += 4) {
        insn = *(uint32_t *)*src;
        if (((insn ^ 0x90000000) & 0x9f000000) == 0) {
            // adrp
            // modify the immediate (len: 4 -> 4)
            int64_t len = (insn >> 29 & 0x3) | ((insn >> 3) & 0x1ffffc);
            len += ((int64_t)*src >> 12) - ((int64_t)*dst >> 12);
            insn &= 0x9f00001f; // clean the immediate
            insn = ((len & 0x3) << 29) | ((len & 0x1ffffc) << 3) | insn;
        }
        *(uint32_t *)*dst = insn;
        *dst += 4;
        *src += 4;
    }
#elif __x86_64__
    struct fde64s insn;
    for (int i = 0; i < min_len; i += insn.len) {
        decode(*src, &insn);
        if (insn.opcode == 0x8b && insn.modrm_rm == RM_DISP32) {
            // mov r64, [rip+]
            // split it into 2 instructions (len: 7 -> 11+3)

            // mov r64 $rip+(immediate)
            *(uint16_t *)*dst = X86_64_MOV_RI64;
            *dst += sizeof(uint16_t);
            *(uint8_t *)*dst = insn.modrm_reg;
            *dst += sizeof(uint8_t);
            *(uint64_t *)*dst = insn.disp32 + (uint64_t)*src + insn.len;
            *dst += sizeof(uint64_t);
            // mov r64 [r64]
            *(uint16_t *)*dst = X86_64_MOV_RM64;
            *dst += sizeof(uint16_t);
            *(uint8_t *)*dst = insn.modrm_reg << 3 | insn.modrm_reg;
            *dst += sizeof(uint8_t);
        }
        else {
            memcpy(*dst, *src, insn.len);
            *dst += insn.len;
        }
        *src += insn.len;
    }
#endif
    mach_vm_protect(mach_task_self(), vmbase, PAGE_SIZE, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
    return;
}

int tiny_hook(void *src, void *dst, void **orig) {
    int kr = 0;
    int jump_size;
    uint8_t jump_insns[MAX_JUMP_SIZE];
    if (orig == NULL) {
        jump_size = calc_jump(jump_insns, src, dst, false);
        kr = write_mem(src, jump_insns, jump_size);
    }
    else {
        // check if the space is enough
        if (!trampo || ((uint64_t)trampo + MAX_JUMP_SIZE + MAX_HEAD_SIZE >= vmbase + PAGE_SIZE)) {
            // alloc a vm to store headers and jumps
            kr = mach_vm_allocate(mach_task_self(), &vmbase, PAGE_SIZE, VM_FLAGS_ANYWHERE);
            if (kr != 0) {
                LOG_ERROR("mach_vm_allocate: %s", mach_error_string(kr));
                return kr;
            }
            trampo = (void *)vmbase;
        }
        void *bak = src;
        *orig = trampo;
        jump_size = calc_jump(jump_insns, src, dst, false);
        save_header(&bak, &trampo, jump_size);
        kr |= write_mem(src, jump_insns, jump_size);
        jump_size += calc_jump(jump_insns, trampo, bak, false);
        kr |= write_mem(trampo, jump_insns, jump_size);
        trampo += jump_size;
    }
    return kr;
}

int tiny_insert(void *src, void *dst) {
    uint8_t jump_insns[MAX_JUMP_SIZE];
    int jump_size = calc_jump(jump_insns, src, dst, true);
    return write_mem(src, jump_insns, jump_size);
}
