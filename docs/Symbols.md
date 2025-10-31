# Symbols

tinyhook includes two symbol resolving function, which can almost resolve all the symbols in the binary

All the resolution is done by parsing the image header and the Mach-O stuctures

## symtbl_solve

```c
void *symtbl_solve(uint32_t image_index, const char *symbol_name);
```

Resolve symbol in the **symbol table** (`LC_SYMTAB`)

#### Parameters
 - `image_index` the index to the image which includes the symbol
 - `symbol_name` raw symbol name (without demangling)

#### Return Value

Return the *actual memory address* of the function on success, or `NULL` if not found

#### Discussion

This is used to resolve unstripped symbols in the binary, usually executable

## symexp_solve

```c
void *symexp_solve(uint32_t image_index, const char *symbol_name);
```

Resolve symbol in the **exported symbol table** (`LC_DYLD_INFO`/`LC_DYLD_EXPORTS_TRIE`)

**Parameters** and **Return Value** are similar to [`symtbl_solve`](#symtbl_solve)

#### Discussion

Usually used for exported symbols in dylib or framework

## Imported Symbol Table

Simply link to the the same binary to get the symbol address

Use [`tiny_interpose`](Hooking.md#tiny_interpose) for replacing function pointers in the imported symbol table
