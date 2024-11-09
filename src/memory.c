#include <mach/mach_init.h> // mach_task_self()
#include <mach/mach_vm.h>   // mach_vm_*
#include <string.h>         // memcpy()

#ifndef COMPACT
#include <mach/mach_error.h> // mach_error_string()
#include <printf.h>          // fprintf()
#endif

#include "../include/tinyhook.h"

int read_mem(void *destnation, const void *source, size_t len) {
    int kr = 0;
    vm_offset_t data;
    mach_msg_type_number_t dataCnt;
    kr |= mach_vm_read(mach_task_self(), (mach_vm_address_t)source, len, &data, &dataCnt);
    destnation = memcpy((unsigned char *)destnation, (unsigned char *)data, dataCnt);
    kr |= mach_vm_deallocate(mach_task_self(), data, dataCnt);
#ifndef COMPACT
    if (kr != 0) {
        fprintf(stderr, "read_mem: %s\n", mach_error_string(kr));
    }
#endif
    return kr;
}

int write_mem(void *destnation, const void *source, size_t len) {
    int kr = 0;
    kr |= mach_vm_protect(mach_task_self(), (mach_vm_address_t)destnation, len, FALSE,
                          VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    kr |= mach_vm_write(mach_task_self(), (mach_vm_address_t)destnation, (vm_offset_t)source, len);
    kr |= mach_vm_protect(mach_task_self(), (mach_vm_address_t)destnation, len, FALSE,
                          VM_PROT_READ | VM_PROT_EXECUTE);
#ifndef COMPACT
    if (kr != 0) {
        fprintf(stderr, "write_mem: %s\n", mach_error_string(kr));
    }
#endif
    return kr;
}
