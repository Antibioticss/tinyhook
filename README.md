# tinyhook

A tiny macOS inline hook framework

## functions

### Inline hook

```c
/* 
hook `function` to `destnation`, store the original pointer in `origin`
- `origin` can be NULL
*/
int tiny_hook(void *function, void *destnation, void **origin);

/* 
insert a jump code at `address`
- `link` == true  -> bl / call
- `link` == false -> b  / jmp
*/
int tiny_insert(void *address, void *destnation, bool link);

/* 
insert a further jump code at `address`
- `link` == true  -> adrp+blr / call [rip]
- `link` == false -> adrp+br  / jmp  [rip]
*/
int tiny_insert_far(void *address, void *destnation, bool link);
```

### Objective-C runtime hook

```c
/* swap two objc methods */
int ocrt_swap(const char *cls1, const char *sel1, const char *cls2, const char *sel2);

/* 
get implemention of an objc method 
- available types are: CLASS_METHOD, INSTANCE_METHOD
*/
void *ocrt_impl(const char *cls, const char *sel, bool type);

/* get method struct from name */
Method ocrt_method(const char *cls, const char *sel, bool type);
```

### Read and write memory

```c
int read_mem(void *destnation, const void *source, size_t len);

int write_mem(void *destnation, const void *source, size_t len);
```

### Get function address from name

```c
void *sym_solve(uint32_t image_index, const char *symbol_name);
```

## examples

```shell
make test
```

Details are in [test](https://github.com/Antibioticss/tinyhook/tree/main/test).

## references

Thanks to these projects for their inspiring idea and code!

- <https://github.com/rodionovd/rd_route>
- <https://github.com/GiveMeZeny/fde64>
