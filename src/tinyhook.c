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
    uint32_t insn = (((int64_t)dst >> 12) - ((int64_t)src >> 12)) & 0x1fffff;
    insn = ((insn & 0x3) << 29) | (insn >> 2 << 5) | AARCH64_ADRP;
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

bool need_far_jump(const void *src, const void *dst) {
    long long distance = dst > src ? dst - src : src - dst;
#ifdef __aarch64__
    return distance >= 128 * MB;
#elif __x86_64__
    return distance >= 2 * GB;
#endif
}

static int calc_jump(uint8_t *output, void *src, void *dst, bool link) {
    if (need_far_jump(src, dst))
        return calc_far_jump(output, src, dst, link);
    else
        return calc_near_jump(output, src, dst, link);
}

static void save_head(void **src, void **dst, int min_len) {
    // return header len (rewritten insns len to dst)
    // int skip_len = 0; // insns(to be overwritten) len from src
    // int head_len = 0; // rewritten insns len to dst
#ifdef __aarch64__
    // skip_len = min_len;
    // head_len = min_len;
    // uint8_t bytes_out[MAX_JUMP_SIZE];
    // read_mem(bytes_out, src, MAX_JUMP_SIZE);
    for (int i = 0; i < min_len; i += 4) {
        uint32_t cur = *(uint32_t *)*src;
        if (((cur ^ 0x90000000) & 0x9f000000) == 0) {
            // adrp
            // modify the immediate
            // TODO: improve this
            int32_t len = (cur >> 29 & 0x3) | ((cur >> 3) & 0x1ffffc);
            len += ((int64_t)*src >> 12) - ((int64_t)*dst >> 12);
            cur &= 0x9f00001f; // clean the immediate
            cur = ((len & 0x3) << 29) | (len >> 2 << 5) | cur;
        }
        *(uint32_t *)*dst = cur;
        *dst += 4;
        *src += 4;
    }
#elif __x86_64__
    struct fde64s insn;
    uint8_t bytes_in[MAX_JUMP_SIZE * 2], bytes_out[MAX_JUMP_SIZE * 4];
    read_mem(bytes_in, src, MAX_JUMP_SIZE * 2);
    for (; skip_len < min_len; skip_len += insn.len) {
        uint64_t cur_addr = (uint64_t)src + skip_len;
        decode(bytes_in + skip_len, &insn);
        if (insn.opcode == 0x8b && insn.modrm_rm == RM_DISP32) {
            // mov r64, [rip+]
            // split it into 2 instructions
            // mov r64 $rip+(immediate)
            // mov r64 [r64]
            *(uint16_t *)(bytes_out + head_len++) = X86_64_MOV_RI64;
            bytes_out[head_len++] += insn.modrm_reg;
            *(uint64_t *)(bytes_out + head_len) = insn.disp32 + cur_addr + insn.len;
            head_len += 8;
            *(uint16_t *)(bytes_out + head_len) = X86_64_MOV_RM64;
            bytes_out[head_len + 2] = insn.modrm_reg << 3 | insn.modrm_reg;
            head_len += 3;
        }
        else {
            memcpy(bytes_out + head_len, bytes_in + skip_len, insn.len);
            head_len += insn.len;
        }
    }
#endif
    // *skip_lenp = skip_len;
    // *head_lenp = head_len;
    // int kr = write_mem(dst, bytes_out, head_len);
    // if (kr != 0) {
    //     LOG_ERROR("save_head: write_mem failed");
    // }
    // return kr;
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
        // static int position = 0;
        static mach_vm_address_t vmbase;
        static void *trampoline;
        if (!trampoline) {
            // alloc a vm to store headers and jumps
            kr = mach_vm_allocate(mach_task_self(), &vmbase, PAGE_SIZE, VM_FLAGS_ANYWHERE);
            if (kr != 0) {
                LOG_ERROR("mach_vm_allocate: %s", mach_error_string(kr));
                return kr;
            }
            trampoline = (void *)vmbase;
        }
        // int skip_len, head_len;
        void *bak = src;
        *orig = trampoline;
        jump_size = calc_jump(jump_insns, src, dst, false);
        mach_vm_protect(mach_task_self(), vmbase, PAGE_SIZE, FALSE, VM_PROT_DEFAULT);
        save_head(&bak, &trampoline, jump_size);
        mach_vm_protect(mach_task_self(), vmbase, PAGE_SIZE, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
        kr |= write_mem(src, jump_insns, jump_size);
        jump_size += calc_jump(jump_insns, trampoline, bak, false);
        kr |= write_mem(trampoline, jump_insns, jump_size);
        trampoline += jump_size;
    }
    return kr;
}

int tiny_insert(void *src, void *dst) {
    uint8_t jump_insns[MAX_JUMP_SIZE];
    int jump_size = calc_jump(jump_insns, src, dst, true);
    return write_mem(src, jump_insns, jump_size);
}
