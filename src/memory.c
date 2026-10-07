#include "private.h"
#include "tinyhook.h"

#include <string.h>                 // memcpy()
#include <libkern/OSCacheControl.h> // sys_icache_invalidate
#include <mach/mach.h>              // vm_region_64(), trunc_page(), round_page()
#ifndef COMPACT
    #include <mach/mach_error.h> // mach_error_string()
#endif

#if TARGET_OS_OSX
    #define mach_vm_protect mach_vm_protect_trap

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
#endif

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
    ARG_CHECK(destination != NULL);
    ARG_CHECK(source != NULL);
    ARG_CHECK(len != 0);
    int kr = 0;
    vm_offset_t data;
    mach_msg_type_number_t dataCnt;
    mach_port_t task = mach_task_self();
    kr = mach_vm_read(task, (mach_vm_address_t)source, len, &data, &dataCnt);
    if (kr != 0) {
        LOG_ERROR("mach_vm_read failed for address %p: %s", source, mach_error_string(kr));
        return kr;
    }
    memcpy(destination, (void *)data, dataCnt);
    kr = mach_vm_deallocate(task, data, dataCnt);
    if (kr != 0) {
        LOG_ERROR("mach_vm_deallocate: %s", mach_error_string(kr));
    }
    return kr;
}

// 该地址所在 region 的 max_protection 是否含 W，顺便回传 max。
// 用来决定 +W 要不要带 VM_PROT_COPY —— 详见 write_mem 的注释。
static int region_max_has_write(const void *addr, vm_prot_t *max_out) {
    vm_address_t a = (vm_address_t)(uintptr_t)addr;
    vm_size_t rsz = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t obj = MACH_PORT_NULL;
    memset(&info, 0, sizeof(info));
    if (vm_region_64(mach_task_self(), &a, &rsz, VM_REGION_BASIC_INFO_64,
                     (vm_region_info_t)&info, &cnt, &obj) != KERN_SUCCESS) {
        if (max_out) *max_out = 0;
        return 0;
    }
    if (max_out) *max_out = info.max_protection;
    return (info.max_protection & VM_PROT_WRITE) != 0;
}

int write_mem(void *destination, const void *source, size_t len) {
    ARG_CHECK(destination != NULL);
    ARG_CHECK(source != NULL);
    ARG_CHECK(len != 0);
    mach_port_t task = mach_task_self();

    // 页对齐的 span：XNU 内部本来就会 trunc/round，显式算出来是为了日志和跨页保护都对。
    const vm_address_t ps = trunc_page((vm_address_t)(uintptr_t)destination);
    const vm_address_t pe = round_page((vm_address_t)(uintptr_t)destination + len);
    const vm_size_t span = pe - ps;

    // ── 要不要带 VM_PROT_COPY ────────────────────────────────────────────────
    // 不能无脑带。iOS 上 __TEXT 的 maxprot=0x5(r-x 不含 W)，只能靠 VM_PROT_COPY
    // 让内核把这页从段里切出来变成私有副本；但切完 max 会被锁成 0x3(rw，X 永久
    // 丢失，且 iOS 不许再拿回来)。于是：
    //   * 同一页第二次 write_mem：页已私有、max 已含 W，再带 COPY 会被内核拒
    //     -> kr=2，写不进去。
    //   * 不带 COPY：私有页 max 已含 W -> 直接成功。
    // 所以按当前 region 的 max 决定，带不带各走各的路。
    vm_prot_t maxp = 0;
    const int need_copy = !region_max_has_write(destination, &maxp);
    vm_prot_t want = VM_PROT_READ | VM_PROT_WRITE;
    if (need_copy) want |= VM_PROT_COPY;

    int kr = mach_vm_protect(task, ps, span, FALSE, want);
    if (kr != 0) {
#ifndef COMPACT
        LOG_ERROR("mach_vm_protect +%c%s failed for %p: %s (region max=%x)",
                  (want & VM_PROT_WRITE) ? 'W' : '-', need_copy ? "|COPY" : "", destination,
                  mach_error_string(kr), (unsigned)maxp);
#endif
        return kr;
    }
    copy_mem(destination, source, len);
    sys_icache_invalidate(destination, len);

    // restore RX：对上面那种 COPY 出来的页必然失败（max=0x3 不含 X），页会留在 rw-。
    // 以前这里把返回值丢了、文档还写着 "set the permission back after writing"，
    // 实际上目标页一旦含代码就会 EXC_BAD_ACCESS。现在如实报出来。
    kr = mach_vm_protect(task, ps, span, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
    if (kr != 0) {
#ifndef COMPACT
        LOG_ERROR("restore RX failed for %p: %s (page left rw-; will fault if it contains code)",
                  destination, mach_error_string(kr));
#endif
    }
    return 0; // 写成功即成功，restore 的结果由上面的日志负责
}
