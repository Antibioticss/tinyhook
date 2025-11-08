# Objective-C

Some simple wrappers for objc runtime functions

## ocrt_hook

```c
int ocrt_hook(const char *cls, const char *sel, void *destination, void **origin);
```

Overwriting the implementation of a method

#### Parameters
 - `cls` class name
 - `sel` selector name
 - `destination` your new implementation
 - `origin` address of the original function pointer

#### Return Value

Return `0` on success

#### Discussion

`origin` can be `NULL` if you don't need to call the original function

## ocrt_swap

```c
int ocrt_swap(const char *cls1, const char *sel1, const char *cls2, const char *sel2);
```

Swap two objc methods

#### Return Value

Return `0` on success

## ocrt_impl + ocrt_method

```c
void *ocrt_impl(char type, const char *cls, const char *sel);

Method ocrt_method(char type, const char *cls, const char *sel);
```

Get implemention or method struct of an objc method

#### Parameters
 - `type` available values: `'+'` (class method), `'-'` (instance method)

#### Return Value

Return `NULL` if not found
