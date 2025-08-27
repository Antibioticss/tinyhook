#include <mach-o/dyld.h>   // _dyld_*
#include <mach-o/loader.h> // mach_header_64, load_command...
#include <mach-o/nlist.h>  // nlist_64
#include <string.h>        // strcmp()

#include "../../include/tinyhook.h"
#include "../private.h"

static void *trie_query(const uint8_t *export, const char *name);

void *symexp_solve(uint32_t image_index, const char *symbol_name) {
    void *symbol_address = NULL;
    intptr_t image_slide = _dyld_get_image_vmaddr_slide(image_index);
    struct mach_header_64 *mh_header = (struct mach_header_64 *)_dyld_get_image_header(image_index);
    struct load_command *ld_command = (void *)mh_header + sizeof(struct mach_header_64);
    if (mh_header == NULL) {
        LOG_ERROR("symexp_solve: image_index out of range!");
    }
    struct dyld_info_command *dyldinfo_cmd = NULL;
    struct segment_command_64 *linkedit_cmd = NULL;
    for (int i = 0; i < mh_header->ncmds; i++) {
        if (ld_command->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *segment = (struct segment_command_64 *)ld_command;
            if (strcmp(segment->segname, "__LINKEDIT") == 0) {
                linkedit_cmd = (struct segment_command_64 *)ld_command;
            }
        } else if (ld_command->cmd == LC_DYLD_INFO_ONLY || ld_command->cmd == LC_DYLD_INFO) {
            dyldinfo_cmd = (struct dyld_info_command *)ld_command;
            if (linkedit_cmd != NULL) {
                break;
            }
        }
        ld_command = (void *)ld_command + ld_command->cmdsize;
    }
    if (dyldinfo_cmd == NULL) {
        LOG_ERROR("symexp_solve: LC_DYLD_INFO_ONLY segment not found!");
        return NULL;
    }
    // stroff and strtbl are in the __LINKEDIT segment
    // Its offset will change when loaded into the memory, so we need to add this slide
    intptr_t linkedit_slide = linkedit_cmd->vmaddr - linkedit_cmd->fileoff;
    uint8_t *export_offset = (uint8_t *)image_slide + linkedit_slide + dyldinfo_cmd->export_off;
    symbol_address = trie_query(export_offset, symbol_name);

    if (symbol_address != NULL) {
        symbol_address += image_slide;
    } else {
        LOG_ERROR("symexp_solve: symbol not found!");
    }
    return symbol_address;
}

inline uint64_t read_uleb128(const uint8_t **p) {
    int bit = 0;
    uint64_t result = 0;
    do {
        uint64_t slice = **p & 0x7f;
        result |= (slice << bit);
        bit += 7;
    } while (*(*p)++ & 0x80);
    return result;
}

static void *trie_query(const uint8_t *export, const char *name) {
    // most comments below are copied from <mach-o/loader.h>, not AI generated :P
    // a trie node starts with a uleb128 stored the lenth of the exported symbol information
    uint64_t node_off = 0;
    const char *rest_name = name;
    void *symbol_address = NULL;
    bool go_child = true;
    while (go_child) {
        const uint8_t *cur_pos = export + node_off;
        uint64_t info_len = read_uleb128(&cur_pos);
        // the exported symbol information is followed by the child edges
        const uint8_t *child_off = cur_pos + info_len;

        if (rest_name[0] == '\0') {
            if (info_len != 0) {
                // first is a uleb128 containing flags
                uint64_t flag = read_uleb128(&cur_pos);
                if (flag == EXPORT_SYMBOL_FLAGS_KIND_REGULAR) {
                    // normally, it is followed by a uleb128 encoded function offset
                    uint64_t symbol_off = read_uleb128(&cur_pos);
                    symbol_address = (void *)symbol_off;
                }
            }
            break;
        } else {
            go_child = false;
            cur_pos = child_off;
            // child edges start with a byte of how many edges (0-255) this node has
            uint8_t child_count = *(uint8_t *)cur_pos++;
            // then followed by each edge.
            for (int i = 0; i < child_count; i++) {
                // each edge is a zero terminated UTF8 of the addition chars
                char *cur_str = (char *)cur_pos;
                size_t cur_len = strlen(cur_str);
                cur_pos += cur_len + 1;
                // then followed by a uleb128 offset for the node that edge points to
                uint64_t next_off = read_uleb128(&cur_pos);
                if (strncmp(rest_name, cur_str, cur_len) == 0) {
                    go_child = true;
                    rest_name += cur_len;
                    node_off = next_off;
                    break;
                }
            }
        }
    }
    return symbol_address;
}
