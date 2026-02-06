#include "tinyhook.h"
#include "private.h"
#ifdef __x86_64__
    #include "fde64/fde64.h"
#endif

#include <stdint.h>
#include <string.h> // memcpy()
#ifndef COMPACT
    #include <mach/mach_error.h> // mach_error_string()
#endif

#ifdef __aarch64__
static inline int32_t sign_extend(uint32_t x, int N) {
    return (int32_t)(x << (32 - N)) >> (32 - N);
}
#endif

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

static int calc_jump(void *output, void *src, void *dst, bool link) {
    if (need_far_jump(src, dst))
        return calc_far_jump(output, src, dst, link);
    else
        return calc_near_jump(output, src, dst, link);
}

static void *trampo;
static mach_vm_address_t vmbase;

static inline void save_header(void **src_p, void **dst_p, int min_len) {
    mach_vm_protect(mach_task_self(), vmbase, PAGE_SIZE, FALSE, VM_PROT_DEFAULT);
#ifdef __aarch64__
    uint32_t insn;
    uint32_t *src = *src_p, *dst = *dst_p;
    for (uint32_t *end = src + min_len / 4; src < end; src++) {
        insn = *src;
        if (((insn ^ 0x90000000) & 0x9f000000) == 0) {
            // adrp
            int32_t imm21 = sign_extend((insn >> 29 & 0x3) | (insn >> 3 & 0x1ffffc), 21);
            int64_t addr = ((int64_t)src >> 12) + imm21;
            int64_t len = addr - ((int64_t)dst >> 12);
            if ((len << 12) < 4 * GB) {
                // modify the immediate (len: 4 -> 4)
                insn &= 0x9f00001f; // clean the immediate
                insn = ((len & 0x3) << 29) | ((len & 0x1ffffc) << 3) | insn;
                *dst++ = insn;
            }
            else {
                // use movz + movk to get the address (len: 4 -> 16)
                int64_t imm64 = addr << 12;
                uint16_t rd = insn & 0b11111;
                bool cleaned = false;
                for (int j = 0; imm64; imm64 >>= 16, j++) {
                    uint64_t cur_imm = imm64 & 0xffff;
                    if (cur_imm) {
                        *dst++ = (j << 21) | (cur_imm << 5) | rd | (cleaned ? AARCH64_MOVK : AARCH64_MOVZ);
                        cleaned = true;
                    }
                }
            }
        }
        else if (((insn ^ 0x14000000) & 0xfc000000) == 0 || ((insn ^ 0x94000000) & 0xfc000000) == 0) {
            // b or bl
            bool link = insn >> 31;
            int32_t imm26 = sign_extend(insn, 26);
            void *addr = (void *)src + (imm26 << 2);
            dst += calc_jump(dst, dst, addr, link) / 4;
        }
        else if (((insn ^ 0x54000000) & 0xff000000) == 0) {
            // b.cond or bc.cond
            int32_t imm19 = sign_extend(insn >> 5, 19);
            void *jmp_dst = (void *)src + (imm19 << 2);
            int jmp_size = calc_jump(dst + 1, dst + 1, jmp_dst, false);
            // clean imm, set new imm, invert cond
            *dst++ = ((insn & 0xff00001f) | ((jmp_size + 4) << 3)) ^ 1;
            dst += jmp_size / 4;
        }
        else {
            *dst++ = insn;
        }
    }
#elif __x86_64__
    struct fde64s insn;
    void *src = *src_p, *dst = *dst_p;
    for (void *end = src + min_len; src < end; src += insn.len) {
        decode(src, &insn);
        if (insn.opcode == 0x8b && insn.modrm_rm == RM_DISP32) {
            // mov r64, [rip+]
            // split it into 2 instructions (len: 7 -> 11+3)

            // mov r64 $rip+(immediate)
            *(uint16_t *)dst = X86_64_MOV_RI64;
            *(uint8_t *)(dst + 1) += insn.modrm_reg;
            dst += sizeof(uint16_t);
            *(uint64_t *)dst = insn.disp32 + (uint64_t)src + insn.len;
            dst += sizeof(uint64_t);
            // mov r64 [r64]
            *(uint16_t *)dst = X86_64_MOV_RM64;
            dst += sizeof(uint16_t);
            *(uint8_t *)dst = insn.modrm_reg << 3 | insn.modrm_reg;
            dst += sizeof(uint8_t);
        }
        else if ((insn.opcode & 0xf0) == 0x70) {
            // jcc (short)
            // invert the condition and insert a jump (len: 2 -> 2+14)
            void *jmp_dst = src + insn.len + insn.imm8;
            int jmp_len = calc_jump(dst + 2, dst + 2, jmp_dst, false);
            *(uint8_t *)dst = insn.opcode ^ 1; // invert the condition
            *(int8_t *)(dst + 1) = jmp_len;
            dst += 2 + jmp_len;
        }
        else if (insn.opcode_len == 2 && insn.opcode == 0x0f && (insn.opcode2 & 0xf0) == 0x80) {
            // jcc (near) (len: 6 -> 2+14)
            void *jmp_dst = src + insn.len + insn.imm32;
            int jmp_len = calc_jump(dst + 2, dst + 2, jmp_dst, false);
            *(uint8_t *)dst = (0x70 | (insn.opcode2 & 0xf)) ^ 1; // invert the condition
            *(int8_t *)(dst + 1) = jmp_len;
            dst += 2 + jmp_len;
        }
        else if (insn.opcode == 0xe8 || insn.opcode == 0xe9) {
            // call or jmp (rel32) (len: 5 -> 5/14)
            bool link = (insn.opcode == 0xe8);
            void *jmp_dst = src + insn.len + insn.imm32;
            dst += calc_jump(dst, dst, jmp_dst, link);
        }
        else {
            memcpy(dst, src, insn.len);
            dst += insn.len;
        }
    }
#endif
    *src_p = src, *dst_p = dst;
    mach_vm_protect(mach_task_self(), vmbase, PAGE_SIZE, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
    return;
}

int tiny_hook(void *src, void *dst, void **orig) {
    ARG_CHECK(src != NULL);
    ARG_CHECK(dst != NULL);
    int kr = 0;
    int jump_size;
    uint8_t jump_insns[MAX_JUMP_SIZE];
    if (orig == NULL) {
        jump_size = calc_jump(jump_insns, src, dst, false);
        return write_mem(src, jump_insns, jump_size);
    }
    // check if the space is enough
    if (!trampo || ((uint64_t)trampo + MAX_JUMP_SIZE + MAX_HEAD_SIZE >= vmbase + PAGE_SIZE)) {
        // alloc a vm to store headers and jumps
        kr = mach_vm_allocate(mach_task_self(), &vmbase, PAGE_SIZE, VM_FLAGS_ANYWHERE);
        if (kr != 0) {
            LOG_ERROR("mach_vm_allocate failed to alloc trampoline: %s", mach_error_string(kr));
            return kr;
        }
        mach_vm_protect(mach_task_self(), vmbase, PAGE_SIZE, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
        trampo = (void *)vmbase;
    }
    void *bak = src;
    *orig = trampo;
    jump_size = calc_jump(jump_insns, src, dst, false);
    save_header(&bak, &trampo, jump_size);
    kr = write_mem(src, jump_insns, jump_size);
    if (kr != 0) return kr;
    jump_size += calc_jump(jump_insns, trampo, bak, false);
    kr = write_mem(trampo, jump_insns, jump_size);
    trampo += jump_size;
    return kr;
}

int tiny_insert(void *src, void *dst) {
    ARG_CHECK(src != NULL);
    ARG_CHECK(dst != NULL);
    uint8_t jump_insns[MAX_JUMP_SIZE];
    int jump_size = calc_jump(jump_insns, src, dst, true);
    return write_mem(src, jump_insns, jump_size);
}
