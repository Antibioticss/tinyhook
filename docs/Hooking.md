# Hooking

tinyhook implemented simple inline hooking and symbol interposing functions, supporting both `arm64` and `x86_64` architecture

Note that inline hooking/inserting only works on **jailbroken iOS devices**, because it requires virtual memory write access

## tiny_hook

```c
int tiny_hook(void *function, void *destination, void **origin);
```

Core **inline hook** function, replace a function with a new implementation

#### Parameters

- `function` the address of the function to be hooked
- `destination` your new implementation of the function
- `origin` pointer to a pointer which will store original function

#### Return Value

Return `0` on success

#### Discussion

`origin` can be `NULL` if you don't need to call the original function

## tiny\_(un)hook_ex

Backup the hook to remove it later

```c
int tiny_hook_ex(th_bak_t *bak, void *function, void *destination, void **origin);

int tiny_unhook_ex(th_bak_t *bak);
```

#### Parameters

- `bak` pointer to `th_bak_t` struct, which includes a backup of original function header

#### Return Value

Return `0` on success

#### Discussion

`tiny_hook_ex` and `tiny_unhook_ex` should be used in pairs

_You should not modify `th_bak_t` struct by yourself!!_

## tiny_interpose

**Interpose a symbol** in a given image

```c
int tiny_interpose(uint32_t image_index, const char *symbol_name, void *replacement, void **origin);
```

#### Parameters

- `image_index` the index to the image which includes the symbol
- `symbol_name` raw symbol name (without demangling)
- `replacement` your new implementation of the function
- `origin` pointer to a pointer which will store original function

#### Return Value

Return `0` on success

#### Discussion

After interposing, all calls to the function with the `symbol_name` would be redirected to `replacement`

Which means, to call the original function, you just need to call it from the current image

This functions works similarly to `fishhook`, by replacing pointers in the imported symbol table

## tiny_insert

Insert a function call

```c
int tiny_insert(void *address, void *destination);
```

#### Parameters

- `address` where to insert the function call
- `destination` the destination of the call

#### Return Value

Return `0` on success

#### Discussion

This will insert a `bl`/`call` instruction at `address` (if it's far it would be `adrp+blr`/`call [rip]`)

Usually used to replace an existing function call
