#include "../private.h"
#include "tinyhook.h"

#include <mach-o/dyld.h>   // _dyld_*
#include <mach-o/loader.h> // mach_header_64, load_command...
#include <mach-o/nlist.h>  // nlist_64
#include <string.h>        // strcmp()

void *symstub_solve(uint32_t image_index, const char *symbol_name) {
    ARG_CHECK(symbol_name != NULL);
    void *symbol_address = NULL;
    intptr_t image_slide = _dyld_get_image_vmaddr_slide(image_index);
    struct mach_header_64 *mh_header = (struct mach_header_64 *)_dyld_get_image_header(image_index);
    struct load_command *ld_command = (void *)mh_header + sizeof(struct mach_header_64);
    if (mh_header == NULL) {
        LOG_ERROR("symstub_solve: image_index %d out of range!", image_index);
    }
    struct section_64 *stub_sect = NULL;
    struct symtab_command *symtab_cmd = NULL;
    struct dysymtab_command *dysymtab_cmd = NULL;
    struct segment_command_64 *linkedit_cmd = NULL;
    for (int i = 0; i < mh_header->ncmds; i++) {
        if (ld_command->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *segment = (struct segment_command_64 *)ld_command;
            if (strcmp(segment->segname, "__TEXT") == 0) {
                struct section_64 *section = (void *)segment + sizeof(struct segment_command_64);
                for (int j = 0; j < segment->nsects; j++) {
                    if ((section[j].flags & SECTION_TYPE) == S_SYMBOL_STUBS) {
                        stub_sect = section + j;
                        break;
                    }
                }
            }
            else if (strcmp(segment->segname, "__LINKEDIT") == 0) {
                linkedit_cmd = (struct segment_command_64 *)ld_command;
            }
        }
        else if (ld_command->cmd == LC_SYMTAB) {
            symtab_cmd = (struct symtab_command *)ld_command;
        }
        else if (ld_command->cmd == LC_DYSYMTAB) {
            dysymtab_cmd = (struct dysymtab_command *)ld_command;
        }
        ld_command = (void *)ld_command + ld_command->cmdsize;
    }
    if (linkedit_cmd == NULL || symtab_cmd == NULL || dysymtab_cmd == NULL || stub_sect == NULL) {
        LOG_ERROR("symstub_solve: bad mach-o structure for image_index %d!", image_index);
        return NULL;
    }
    void *linkedit_base = (void *)image_slide + linkedit_cmd->vmaddr - linkedit_cmd->fileoff;
    char *str_tbl = linkedit_base + symtab_cmd->stroff;
    struct nlist_64 *nl_tbl = linkedit_base + symtab_cmd->symoff;
    uint32_t *indirect_sym_tbl = linkedit_base + dysymtab_cmd->indirectsymoff;

    uint32_t stub_len = stub_sect->reserved2;
    uint32_t stub_count = stub_sect->size / stub_len;
    uint32_t *indirect_sym_entry = indirect_sym_tbl + stub_sect->reserved1;
    for (int i = 0; i < stub_count; i++) {
        uint32_t symtab_index = indirect_sym_entry[i];
        if (symtab_index == INDIRECT_SYMBOL_LOCAL || symtab_index == (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
            continue;
        }
        if (strcmp(symbol_name, str_tbl + nl_tbl[symtab_index].n_un.n_strx) == 0) {
            symbol_address = (void *)stub_sect->addr + i * stub_len;
        }
    }
    if (symbol_address != NULL) {
        symbol_address += image_slide;
    }
    else {
        LOG_ERROR("symstub_solve: symbol '%s' not found in image_index %d!", symbol_name, image_index);
    }
    return symbol_address;
}
