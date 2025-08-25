# tinyhook

A tiny macOS inline hook framework

## Functions

### Inline hook

```c
/*
hook `function` to `destination`, store the original pointer in `origin`
- `origin` can be NULL
*/
int tiny_hook(void *function, void *destination, void **origin);

/*
insert a jump code at `address`
- `link` == true  -> bl / call
- `link` == false -> b  / jmp
*/
int tiny_insert(void *address, void *destination, bool link);

/*
insert a further jump code at `address`
- `link` == true  -> adrp+blr / call [rip]
- `link` == false -> adrp+br  / jmp  [rip]
*/
int tiny_insert_far(void *address, void *destination, bool link);
```

### Objective-C runtime hook

```c
/* similar to `tiny_hook`, but for objc */
int ocrt_hook(const char *cls, const char *sel, void *destination, void **origin);

/* swap two objc methods */
int ocrt_swap(const char *cls1, const char *sel1, const char *cls2, const char *sel2);

/*
get implemention of an objc method
- available types are: '+' (class method), '-' (instance method)
*/
void *ocrt_impl(char type, const char *cls, const char *sel);

/* get method struct pointer from name */
Method ocrt_method(char type, const char *cls, const char *sel);
```

### Read and write memory

```c
int read_mem(void *destination, const void *source, size_t len);

int write_mem(void *destination, const void *source, size_t len);
```

### Get function address from name

```c
/* get symbol address from symbol table (LC_SYMTAB) */
void *symtbl_solve(uint32_t image_index, const char *symbol_name);

/* get symbol address from export table (LC_DYLD_INFO) */
void *symexp_solve(uint32_t image_index, const char *symbol_name);
```

## Examples

```shell
make test
```

Details are in [test](https://github.com/Antibioticss/tinyhook/tree/main/test).

## References

Thanks to these projects for their inspiring idea and code!

- <https://github.com/rodionovd/rd_route>
- <https://github.com/GiveMeZeny/fde64>
