# tinyhook

A tiny macOS inline hook framework

**Now support both ARM64 and x86_64!**

## functions

### read and write memory

```c
int read_mem(void *destnation, const void *source, size_t len);
int write_mem(void *destnation, const void *source, size_t len);
```

### insert a jump at an address

`tiny_insert()` uses `bl` or `b` (depends on `link` flag) on ARM64, and `jmp` or `call` on x86_64

`tiny_isnert_far()` uses `adrp` + `blr`/`br` on ARM64, and `jmp` or call on x86_64

```c
int tiny_insert(void *address, void *destnation, bool link);
int tiny_insert_far(void *address, void *destnation, bool link);
```

### inline hook

`origin` can be `NULL`

```c
int tiny_hook(void *function, void *destnation, void **origin);
```

### get function address from name

```c
void *sym_solve(uint32_t image_index, const char *symbol_name);
```

## examples

See [tests/example.c](https://github.com/Antibioticss/tinyhook/blob/main/test/example.c)

## references

Thanks to these projects for their inspiring idea and code!

-   https://github.com/rodionovd/rd_route
-   https://github.com/GiveMeZeny/fde64
