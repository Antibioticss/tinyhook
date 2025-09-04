#include <mach-o/dyld.h>   // _dyld_*
#include <mach-o/loader.h> // mach_header_64, load_command...
#include <mach-o/nlist.h>  // nlist_64
#include <string.h>        // strcmp()

#include "../../include/tinyhook.h"
#include "../private.h"

static uint64_t read_uleb128(const uint8_t **p) {
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
    // documents in <mach-o/loader.h>
    uint64_t node_off = 0;
    const char *rest_name = name;
    void *symbol_address = NULL;
    bool go_child = true;
    while (go_child) {
        const uint8_t *cur_pos = export + node_off;
        uint64_t info_len = read_uleb128(&cur_pos);
        const uint8_t *child_off = cur_pos + info_len;
        if (rest_name[0] == '\0') {
            if (info_len != 0) {
                uint64_t flag = read_uleb128(&cur_pos);
                if (flag == EXPORT_SYMBOL_FLAGS_KIND_REGULAR) {
                    uint64_t symbol_off = read_uleb128(&cur_pos);
                    symbol_address = (void *)symbol_off;
                }
            }
            break;
        }
        else {
            go_child = false;
            cur_pos = child_off;
            uint8_t child_count = *(uint8_t *)cur_pos++;
            for (int i = 0; i < child_count; i++) {
                char *cur_str = (char *)cur_pos;
                size_t cur_len = strlen(cur_str);
                cur_pos += cur_len + 1;
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
    struct linkedit_data_command *export_trie = NULL;
    for (int i = 0; i < mh_header->ncmds; i++) {
        if (ld_command->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *segment = (struct segment_command_64 *)ld_command;
            if (strcmp(segment->segname, "__LINKEDIT") == 0) {
                linkedit_cmd = (struct segment_command_64 *)ld_command;
            }
        }
        else if (ld_command->cmd == LC_DYLD_INFO_ONLY || ld_command->cmd == LC_DYLD_INFO) {
            dyldinfo_cmd = (struct dyld_info_command *)ld_command;
            if (linkedit_cmd != NULL) break;
        }
        else if (ld_command->cmd == LC_DYLD_EXPORTS_TRIE) {
            export_trie = (struct linkedit_data_command *)ld_command;
            if (linkedit_cmd != NULL) break;
        }
        ld_command = (void *)ld_command + ld_command->cmdsize;
    }
    if (linkedit_cmd == NULL) {
        LOG_ERROR("symexp_solve: __LINKEDIT segment not found!");
        return NULL;
    }
    // stroff and strtbl are in the __LINKEDIT segment
    // Its offset will change when loaded into the memory, so we need to add this slide
    uint64_t linkedit_base = image_slide + linkedit_cmd->vmaddr - linkedit_cmd->fileoff;
    uint8_t *export_offset;
    if (dyldinfo_cmd != NULL)
        export_offset = (uint8_t *)linkedit_base + dyldinfo_cmd->export_off;
    else if (export_trie != NULL)
        export_offset = (uint8_t *)linkedit_base + export_trie->dataoff;
    else {
        LOG_ERROR("symexp_solve: neither LC_DYLD_INFO_ONLY nor LC_DYLD_EXPORTS_TRIE load command found!");
        return NULL;
    }
    symbol_address = trie_query(export_offset, symbol_name);

    if (symbol_address != NULL) {
        symbol_address += (uint64_t)mh_header;
    }
    else {
        LOG_ERROR("symexp_solve: symbol not found!");
    }
    return symbol_address;
}
