#include <objc/runtime.h> // objc_*, ...

#ifndef COMPACT
#include <mach/mach_error.h> // mach_error_string()
#include <printf.h>          // fprintf()
#endif

#include "../include/tinyhook.h"

Method ocrt_method(char type, const char *cls, const char *sel) {
    Method oc_method = NULL;
    Class oc_class = objc_getClass(cls);
    SEL oc_selector = sel_registerName(sel);
    switch (type) {
    case '+':
        oc_method = class_getClassMethod(oc_class, oc_selector);
        break;
    case '-':
        oc_method = class_getInstanceMethod(oc_class, oc_selector);
        break;
    default:
#ifndef COMPACT
        fprintf(stderr, "ocrt_method: invalid method type: %d\n", type);
#endif
        break;
    }
    return oc_method;
}

void *ocrt_impl(char type, const char *cls, const char *sel) {
    return method_getImplementation(ocrt_method(type, cls, sel));
}

static inline Method ensure_method(const char *cls, const char *sel) {
    Method oc_method = ocrt_method('+', cls, sel);
    if (oc_method == NULL) {
        oc_method = ocrt_method('-', cls, sel);
    }
#ifndef COMPACT
    if (oc_method == NULL) {
        fprintf(stderr, "ensure_method: method not found!\n");
    }
#endif
    return oc_method;
}

int ocrt_swap(const char *cls1, const char *sel1, const char *cls2, const char *sel2) {
    Method oc_method1 = ensure_method(cls1, sel1);
    Method oc_method2 = ensure_method(cls2, sel2);
    if (oc_method1 == NULL || oc_method2 == NULL) {
        return 1;
    }
    method_exchangeImplementations(oc_method1, oc_method2);
    return 0;
}

int ocrt_hook(const char *cls, const char *sel, void *destination, void **origin) {
    Method oc_method = ensure_method(cls, sel);
    if (oc_method == NULL) {
        return 1;
    }
    void *origin_imp = method_setImplementation(oc_method, destination);
    if (origin != NULL) {
        *origin = origin_imp;
    }
    return 0;
}
