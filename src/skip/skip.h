#ifndef skip_h
#define skip_h

#include <stddef.h>

#define ASIZE 0x100

typedef long long offset_t;
typedef struct {
    int val;
    int nxt;
} node_t;
typedef struct {
    size_t plen;
    unsigned char *patt;
    int *buck;
    node_t *buff;
} skipidx_t;

void skip_init(skipidx_t *idx, size_t patlen, const unsigned char *pattern);

int skip_match(skipidx_t *idx, unsigned char *start, unsigned char *end, int count, offset_t *offs);

void skip_release(skipidx_t *idx);

#endif
