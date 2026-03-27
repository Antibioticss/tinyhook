#include "tinyhook.h"

#include <mach-o/dyld.h>
#include <stdio.h>
#include <string.h>

static uint32_t name2index(const char *image_name) {
    size_t len = strlen(image_name);
    uint32_t total = _dyld_image_count();
    for (uint32_t i = 0; i < total; i++) {
        const char *full_name = _dyld_get_image_name(i);
        size_t full_len = strlen(full_name);
        if (len <= full_len && strcmp(image_name, full_name + full_len - len) == 0) {
            return i;
        }
    }
    return -1;
}

__attribute__((constructor(0))) void load() {
    FILE *fp = fopen("external_symbol.txt", "r");
    char symbol[1024];
    uint32_t idx = name2index("sublime_text");
    if (idx == -1) return;
    int total = 0;
    int found = 0;
    while (fscanf(fp, "%s", symbol) != EOF) {
        total++;
        void *addr = symbol_resolve(idx, symbol, RESOLVE_EXPORT);
        fprintf(stderr, "%p %s\n", addr, symbol);
        if (addr) found++;
    }
    printf("total: %d\nfound: %d\n", total, found);
}
