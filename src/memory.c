#include "../include/tinyhook.h"
#include "private.h"

#include <string.h> // memcpy()
#ifndef COMPACT
#include <mach/mach_error.h> // mach_error_string()
#endif

int read_mem(void *destination, const void *source, size_t len) {
    int kr = 0;
    vm_offset_t data;
    mach_msg_type_number_t dataCnt;
    kr |= mach_vm_read(mach_task_self(), (mach_vm_address_t)source, len, &data, &dataCnt);
    memcpy((unsigned char *)destination, (unsigned char *)data, dataCnt);
    kr |= mach_vm_deallocate(mach_task_self(), data, dataCnt);
    if (kr != 0) {
        LOG_ERROR("read_mem: %s", mach_error_string(kr));
    }
    return kr;
}

int write_mem(void *destination, const void *source, size_t len) {
    int kr = 0;
    kr |= mach_vm_protect(mach_task_self(), (mach_vm_address_t)destination, len, FALSE,
                          VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    kr |= mach_vm_write(mach_task_self(), (mach_vm_address_t)destination, (vm_offset_t)source, len);
    kr |= mach_vm_protect(mach_task_self(), (mach_vm_address_t)destination, len, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
    if (kr != 0) {
        LOG_ERROR("write_mem: %s", mach_error_string(kr));
    }
    return kr;
}
