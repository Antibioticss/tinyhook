#include "tinyhook.h"

#include <string.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <mach-o/loader.h>

static void *solve_mach_header(const struct mach_header_64* header, const char* symbol_name) {
    void *symbol_address = NULL;
    const struct load_command* command = (void*)header + sizeof(struct mach_header_64);
    for (int i = 0; i < header->ncmds; i++) {
        if (command->cmd == LC_SYMTAB) {
            const struct symtab_command* symtab_cmd = (struct symtab_command*)command;
            const struct nlist_64* nl_tbl = (void*)header + symtab_cmd->symoff;
            const char* str_tbl = (void*)header + symtab_cmd->stroff;
            for (int j = 0; j < symtab_cmd->nsyms; j++) {
                if (strcmp(symbol_name, str_tbl+nl_tbl[j].n_un.n_strx) == 0) {
                    if ((nl_tbl[j].n_type & N_TYPE) == N_SECT) {
                        symbol_address = (void*)nl_tbl[j].n_value;
                        break;
                    }
                }
            }
            break;
        }
        command = (void*)command + command->cmdsize;
    }
    return symbol_address;
}

void *sym_solve(uint32_t image_index, const char* symbol_name) {
    void *symbol_address = solve_mach_header((struct mach_header_64*)_dyld_get_image_header(image_index), symbol_name);
    if (symbol_address) symbol_address += _dyld_get_image_vmaddr_slide(image_index);
    return symbol_address;
}