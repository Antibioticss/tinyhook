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

`tiny_isnert_far()`  uses `adrp` + `blr`/`br` on ARM64, and `jmp` or call on x86_64

```c
int tiny_insert(void *address, void *destnation, bool link);
int tiny_insert_far(void *address, void *destnation, bool link);
```

### inline hook

`origin` can be `NULL`

```c
int tiny_hook(void *function, void *destnation, void **origin);
```

## examples

```c
#include <stdio.h>
#include <mach-o/dyld.h>

#include "tinyhook.h"

uint32_t (*orig_dyld_image_count)(void);
uint32_t my_dyld_image_count(void) {
	uint32_t image_count = orig_dyld_image_count();
	printf("hooked _dyld_image_count: %d!\n", image_count);
	return 5;
}

int main() {
	int image_count = _dyld_image_count();
	for (int i = 0; i < image_count; i++) {
		printf("image[%d]: %s\n", i, _dyld_get_image_name(i));
	}

	tiny_hook(_dyld_image_count, my_dyld_image_count, (void **)&orig_dyld_image_count);

	int image_count = _dyld_image_count();
	for (int i = 0; i < image_count; i++) {
		printf("image[%d]: %s\n", i, _dyld_get_image_name(i));
	}
	return 0;
}
```

## references

Thanks to these projects for their inspiring idea and code!

- https://github.com/rodionovd/rd_route
- https://github.com/GiveMeZeny/fde64
