#include <mach/mach_init.h> // mach_task_self()
#include <mach/mach_vm.h>   // mach_vm_*
#include <stdint.h>
#include <string.h> // memcpy()

#ifdef __x86_64__
#include "fde64/fde64.h"
#endif

#include "../include/tinyhook.h"
#include "private.h"

int tiny_insert(void *address, void *destination, bool link) {
    size_t jump_size;
    uint8_t bytes[MAX_JUMP_SIZE];
#ifdef __aarch64__
    // b/bl imm    ; go to destination
    jump_size = 4;
    uint32_t assembly;
    assembly = (destination - address) >> 2 & 0x3ffffff;
    assembly |= link ? AARCH64_BL : AARCH64_B;
    *(uint32_t *)bytes = assembly;
#elif __x86_64__
    // jmp/call imm    ; go to destination
    jump_size = 5;
    *bytes = link ? X86_64_CALL : X86_64_JMP;
    *(int32_t *)(bytes + 1) = (int64_t)destination - (int64_t)address - 5;
#endif
    write_mem(address, bytes, jump_size);
    return jump_size;
}

int tiny_insert_far(void *address, void *destination, bool link) {
    size_t jump_size;
    uint8_t bytes[MAX_JUMP_SIZE];
#ifdef __aarch64__
    // adrp    x17, imm
    // add     x17, x17, imm    ; x17 -> destination
    // br/blr  x17
    jump_size = 12;
    uint32_t assembly;
    assembly = (((int64_t)destination >> 12) - ((int64_t)address >> 12)) & 0x1fffff;
    assembly = ((assembly & 0x3) << 29) | (assembly >> 2 << 5) | AARCH64_ADRP;
    *(uint32_t *)bytes = assembly;
    assembly = ((int64_t)destination & 0xfff) << 10 | AARCH64_ADD;
    *(uint32_t *)(bytes + 4) = assembly;
    *(uint32_t *)(bytes + 8) = link ? AARCH64_BLR : AARCH64_BR;
#elif __x86_64__
    jump_size = 14;
    // jmp [rip]    ; rip stored destination
    *(uint32_t *)bytes = link ? X86_64_CALL_RIP : X86_64_JMP_RIP;
    bytes[5] = bytes[6] = 0;
    *(uint64_t *)(bytes + 6) = (uint64_t)destination;
#endif
    write_mem(address, bytes, jump_size);
    return jump_size;
}

int position = 0;
mach_vm_address_t vm;

static int get_jump_size(void *address, void *destination);
static int insert_jump(void *address, void *destination);
static int save_header(void *address, void *destination, int *skip_len);

int tiny_hook(void *function, void *destination, void **origin) {
    int kr = 0;
    if (origin == NULL)
        insert_jump(function, destination);
    else {
        if (!position) {
            // alloc a vm to store headers and jumps
            kr = mach_vm_allocate(mach_task_self(), &vm, PAGE_SIZE, VM_FLAGS_ANYWHERE);
            if (kr != 0) {
                LOG_ERROR("mach_vm_allocate: %s", mach_error_string(kr));
            }
        }
        int skip_len;
        *origin = (void *)(vm + position);
        position += save_header(function, (void *)(vm + position), &skip_len);
        position += insert_jump((void *)(vm + position), function + skip_len);
        insert_jump(function, destination);
    }
    return kr;
}

static int get_jump_size(void *address, void *destination) {
    long long distance = destination > address ? destination - address : address - destination;
#ifdef __aarch64__
    return distance < 128 * MB ? 4 : 12;
#elif __x86_64__
    return distance < 2 * GB ? 5 : 14;
#endif
}

static int insert_jump(void *address, void *destination) {
    if (get_jump_size(address, destination) <= 5)
        return tiny_insert(address, destination, false);
    else
        return tiny_insert_far(address, destination, false);
}

static int save_header(void *address, void *destination, int *skip_len) {
    int header_len = 0;
#ifdef __aarch64__
    header_len = *skip_len = get_jump_size(address, destination);
    uint8_t bytes_out[MAX_JUMP_SIZE];
    read_mem(bytes_out, address, MAX_JUMP_SIZE);
    for (int i = 0; i < header_len; i += 4) {
        uint32_t cur_asm = *(uint32_t *)(bytes_out + i);
        int64_t cur_addr = (int64_t)address + i, cur_dst = (int64_t)destination + i;
        if (((cur_asm ^ 0x90000000) & 0x9f000000) == 0) {
            // adrp
            // modify the immediate
            int32_t len = (cur_asm >> 29 & 0x3) | ((cur_asm >> 3) & 0x1ffffc);
            len += (cur_addr >> 12) - (cur_dst >> 12);
            cur_asm &= 0x9f00001f; // clean the immediate
            cur_asm = ((len & 0x3) << 29) | (len >> 2 << 5) | cur_asm;
            *(uint32_t *)(bytes_out + i) = cur_asm;
        }
    }
#elif __x86_64__
    int min_len;
    struct fde64s assembly;
    uint8_t bytes_in[MAX_JUMP_SIZE * 2], bytes_out[MAX_JUMP_SIZE * 4];
    read_mem(bytes_in, address, MAX_JUMP_SIZE * 2);
    min_len = get_jump_size(address, destination);
    for (*skip_len = 0; *skip_len < min_len; *skip_len += assembly.len) {
        uint64_t cur_addr = (uint64_t)address + *skip_len;
        decode(bytes_in + *skip_len, &assembly);
        if (assembly.opcode == 0x8B && assembly.modrm_rm == RM_DISP32) {
            // mov r64, [rip+]
            // split it into 2 instructions
            // mov r64 $rip+(immediate)
            // mov r64 [r64]
            *(uint16_t *)(bytes_out + header_len) = X86_64_MOV_RI64;
            bytes_out[header_len + 1] += assembly.modrm_reg;
            *(uint64_t *)(bytes_out + header_len + 2) = assembly.disp32 + cur_addr + assembly.len;
            header_len += 10;
            *(uint16_t *)(bytes_out + header_len) = X86_64_MOV_RM64;
            bytes_out[header_len + 2] = assembly.modrm_reg << 3 | assembly.modrm_reg;
            header_len += 3;
        } else {
            memcpy(bytes_out + header_len, bytes_in + *skip_len, assembly.len);
            header_len += assembly.len;
        }
    }
#endif
    write_mem(destination, bytes_out, header_len);
    return header_len;
}
