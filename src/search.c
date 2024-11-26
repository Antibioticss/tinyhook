#ifndef COMPACT
#include <printf.h> // fprintf()
#endif

#include "skip/skip.h"

#include "../include/tinyhook.h"

int find_data(void *start, void *end, const unsigned char *data, size_t len, int count, void **out) {
    int matched;
    skipidx_t idx;
    skip_init(&idx, len, data);
    matched = skip_match(&idx, start, end, count, (offset_t *)out);
    skip_release(&idx);
#ifndef COMPACT
    if (matched == 0) {
        fprintf(stderr, "find_data: data not found!\n");
    }
#endif
    return matched;
}

// int find_code(uint32_t image_index, const unsigned char *code, size_t len, int count, void **out);
