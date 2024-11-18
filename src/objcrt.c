#include <objc/objc-runtime.h>
#include <objc/runtime.h>

#ifndef COMPACT
#include <mach/mach_error.h> // mach_error_string()
#include <printf.h>          // fprintf()
#endif

#include "../include/tinyhook.h"

Method ocrt_method(const char *cls, const char *sel, bool type) {
    Method oc_method = NULL;
    Class oc_class = objc_getClass(cls);
    SEL oc_selector = sel_registerName(sel);
    if (type == CLASS_METHOD) {
        oc_method = class_getClassMethod(oc_class, oc_selector);
    } else if (type == INSTANCE_METHOD) {
        oc_method = class_getInstanceMethod(oc_class, oc_selector);
    }
#ifndef COMPACT
    else {
        fprintf(stderr, "invalid method type: %d\n", type);
    }
#endif
    return oc_method;
}

void *ocrt_impl(const char *cls, const char *sel, bool type) {
    return method_getImplementation(ocrt_method(cls, sel, type));
}

static Method ensure_method(const char *cls, const char *sel);

int ocrt_swap(const char *cls1, const char *sel1, const char *cls2, const char *sel2) {
    Method oc_method1 = ensure_method(cls1, sel1);
    Method oc_method2 = ensure_method(cls2, sel2);
    if (oc_method1 == NULL || oc_method2 == NULL) {
        return 1;
    }
    method_exchangeImplementations(oc_method1, oc_method2);
    return 0;
}

int ocrt_hook(const char *cls, const char *sel, void *destnation, void **origin) {
    Method oc_method = ensure_method(cls, sel);
    if (oc_method == NULL) {
        return 1;
    }
    *origin = method_setImplementation(oc_method, destnation);
    return 0;
}

static Method ensure_method(const char *cls, const char *sel) {
    Method oc_method = ocrt_method(cls, sel, CLASS_METHOD);
    if (oc_method == NULL) {
        oc_method = ocrt_method(cls, sel, INSTANCE_METHOD);
    }
#ifndef COMPACT
    if (oc_method == NULL) {
        fprintf(stderr, "method not found!\n");
    }
#endif
    return oc_method;
}
