#include "../include/tinyhook.h"

#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <string.h>

void *sym_solve(uint32_t image_index, const char *symbol_name) {
    void *symbol_address = NULL;
    intptr_t image_slide = _dyld_get_image_vmaddr_slide(image_index);
    struct mach_header_64 *mh_header = (struct mach_header_64 *)_dyld_get_image_header(image_index);
    struct load_command *ld_command = (void *)mh_header + sizeof(struct mach_header_64);
#ifdef debug
    assert(image_slide);
    assert(mh_header);
#endif
    struct symtab_command *symtab_cmd = NULL;
    struct segment_command_64 *linkedit_cmd = NULL;
    for (int i = 0; i < mh_header->ncmds; i++) {
        if (ld_command->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *segment = (struct segment_command_64 *)ld_command;
            if (strcmp(segment->segname, "__LINKEDIT") == 0) {
                linkedit_cmd = (struct segment_command_64 *)ld_command;
            }
        } else if (ld_command->cmd == LC_SYMTAB) {
            symtab_cmd = (struct symtab_command *)ld_command;
            if (linkedit_cmd != NULL) {
                break;
            }
        }
        ld_command = (void *)ld_command + ld_command->cmdsize;
    }
    // stroff and strtbl are in the __LINKEDIT segment
    // Its offset will change when loaded into the memory, so we need to add this slide
    intptr_t linkedit_slide = linkedit_cmd->vmaddr - linkedit_cmd->fileoff;
    struct nlist_64 *nl_tbl = (void *)image_slide + linkedit_slide + symtab_cmd->symoff;
    char *str_tbl = (void *)image_slide + linkedit_slide + symtab_cmd->stroff;
    for (int j = 0; j < symtab_cmd->nsyms; j++) {
        if ((nl_tbl[j].n_type & N_TYPE) == N_SECT) {
            if (strcmp(symbol_name, str_tbl + nl_tbl[j].n_un.n_strx) == 0) {
                symbol_address = (void *)nl_tbl[j].n_value;
                break;
            }
        }
    }
    if (symbol_address != NULL) {
        symbol_address += image_slide;
    }
    return symbol_address;
}
