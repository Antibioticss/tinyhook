#include <stdlib.h>
#include <string.h>

#include "skip.h"

void skip_init(skipidx_t *idx, size_t patlen, unsigned char *pattern) {
    int bufidx = 1;
    unsigned char *patt = (unsigned char *)malloc((patlen + 1) * sizeof(unsigned char));
    int *bucket = (int *)malloc(ASIZE * sizeof(int));
    node_t *buffer = (node_t *)malloc((patlen + 1) * sizeof(node_t));
    memcpy(patt, pattern, patlen);
    memset(bucket, 0, ASIZE * sizeof(int));
    for (int i = 0; i < patlen; i++) {
        buffer[bufidx].val = i;
        buffer[bufidx].nxt = bucket[pattern[i]];
        bucket[pattern[i]] = bufidx;
        bufidx++;
    }
    idx->plen = patlen;
    idx->patt = patt;
    idx->buck = bucket;
    idx->buff = buffer;
    return;
}

int skip_match(skipidx_t *idx, unsigned char *start, unsigned char *end, int count, offset_t *offs) {
    size_t patlen = idx->plen;
    unsigned char *pattern = idx->patt;
    int *bucket = idx->buck;
    node_t *buffer = idx->buff;
    int matched = 0;
    unsigned char *edge = end - patlen - 1;
    unsigned char *chbase = start + patlen - 1;
    for (; chbase <= edge; chbase += patlen) {
        for (int j = bucket[*chbase]; buffer[j].nxt; j = buffer[j].nxt) {
            unsigned char *cur = chbase - buffer[j].val;
            if (memcmp(pattern, cur, patlen) == 0) {
                offs[matched++] = (offset_t)cur;
                if (matched == count) {
                    goto match_ended;
                }
            }
        }
    }
    for (int i = bucket[*chbase]; buffer[i].nxt; i = buffer[i].nxt) {
        unsigned char *cur = chbase - buffer[i].val;
        if (cur + patlen <= end) {
            if (memcmp(pattern, cur, patlen) == 0) {
                offs[matched++] = (offset_t)cur;
                if (matched == count) {
                    goto match_ended;
                }
            }
        }
    }
match_ended:
    return matched;
}

void skip_release(skipidx_t *idx) {
    free(idx->patt);
    free(idx->buck);
    free(idx->buff);
    idx->patt = NULL;
    idx->buck = NULL;
    idx->buff = NULL;
    return;
}
