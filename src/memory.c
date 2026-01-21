#include "private.h"
#include "tinyhook.h"

#include <string.h> // memcpy()
#ifndef COMPACT
    #include <mach/mach_error.h> // mach_error_string()
#endif

__attribute__((naked)) static kern_return_t mach_vm_protect_trap(mach_port_name_t task, mach_vm_address_t address,
                                                                 mach_vm_size_t size, boolean_t set_maximum,
                                                                 vm_prot_t new_protection) {
#ifdef __aarch64__
    asm volatile("mov    x16, #-0xe\n"
                 "svc    #0x80\n"
                 "ret");
#elif __x86_64__
    asm volatile(".intel_syntax\n"
                 "mov    r10, rcx\n"
                 "mov    eax, 0x100000e\n"
                 "syscall\n"
                 "ret");
#endif
}

static inline void copy_mem(void *destination, const void *source, size_t len) {
    unsigned char *dst = (unsigned char *)destination;
    const unsigned char *src = (const unsigned char *)source;
    while (len >= 16) {
        __builtin_memcpy(dst, src, 16); // will be optimized using registers
        dst += 16, src += 16, len -= 16;
    }
    if (len & 8) {
        __builtin_memcpy(dst, src, 8);
        dst += 8, src += 8, len ^= 8;
    }
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }
}

int read_mem(void *destination, const void *source, size_t len) {
    int kr = 0;
    vm_offset_t data;
    mach_msg_type_number_t dataCnt;
    kr = mach_vm_read(mach_task_self(), (mach_vm_address_t)source, len, &data, &dataCnt);
    if (kr != 0) {
        LOG_ERROR("mach_vm_read: %s", mach_error_string(kr));
        return kr;
    }
    memcpy((void *)destination, (void *)data, dataCnt);
    kr = mach_vm_deallocate(mach_task_self(), data, dataCnt);
    if (kr != 0) {
        LOG_ERROR("mach_vm_deallocate: %s", mach_error_string(kr));
    }
    return kr;
}

int write_mem(void *destination, const void *source, size_t len) {
    int kr = 0;
    kr = mach_vm_protect_trap(mach_task_self(), (mach_vm_address_t)destination, len, FALSE,
                              VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    if (kr != 0) {
        LOG_ERROR("mach_vm_protect: %s", mach_error_string(kr));
        return kr;
    }
    copy_mem(destination, source, len);
    kr = mach_vm_protect_trap(mach_task_self(), (mach_vm_address_t)destination, len, FALSE,
                              VM_PROT_READ | VM_PROT_EXECUTE);
    if (kr != 0) {
        LOG_ERROR("mach_vm_protect: %s", mach_error_string(kr));
    }
    return kr;
}
