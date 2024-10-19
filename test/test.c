#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "tinyhook.h"

int (*orig_printf)(const char *format, ...);
int my_printf(const char *format, ...) {
	if (strcmp(format, "Hello, world!\n") == 0) {
		return orig_printf("=== printf hooked!\nHello, tinyhook!\n");
	}
	va_list args;
	va_start(args, format);
	return vprintf(format, args);
}

int fake_add(int a, int b) {
	printf("=== calling fake_add with: %d, %d\n", a, b);
	return -1;
}

__attribute__((constructor(0))) int load() {
	printf("=== libtest loading...\n");

	void* func_add = sym_solve(0, "_add");
	printf("=== add() address: %p\n", func_add);
	tiny_insert(func_add, fake_add, false);

	tiny_hook(printf, my_printf, (void**)&orig_printf);
	return 0;
}
