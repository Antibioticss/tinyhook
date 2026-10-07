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

### iOS / `__TEXT` caveat

`__TEXT` is mapped `r-x` (`maxprot=0x5`), so the first write to a page has to pass
`VM_PROT_COPY` to make the kernel copy that page out of the segment. That copy is one-shot:
the resulting private page ends up with `max=rw-` — write is allowed, execute is dropped
permanently (iOS does not allow taking it back). Therefore:

- further writes to the same page are performed without `VM_PROT_COPY`, based on the region's
  `max_protection`
- **restoring `RX` fails on such pages and they stay `rw-`** — this is reported through
  `LOG_ERROR` instead of being swallowed. Only write to pages that do not need to be
  executed afterwards (data, or code you are explicitly replacing).

On pages where `RX` is still allowed (e.g. `max=rwx`) the permission is restored as
documented above.

