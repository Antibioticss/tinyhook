# Memory

tinyhook provided simple apis for memory access

## read_mem + write_mem

```c
int read_mem(void *destination, const void *source, size_t len);

int write_mem(void *destination, const void *source, size_t len);
```

Both functions copy memory data from a `source` to a `destination`

**Parameters**

- `destination` address to write the data to
- `source` address to read the data from
- `len` length of the data in bytes

**Return Value**

Return `0` on success

**Discussion**

`read_mem` is just a simple wrapper for `mach_vm_read`.

`write_mem` will change the permission of the memory page to writable, and set the permission back after writing

It can be used to patch instructions in memory.
