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
	for (int i = 0; i < _dyld_image_count(); i++) {
		printf("image[%d]: %s\n", i, _dyld_get_image_name(i));
	}

	tiny_hook(_dyld_image_count, my_dyld_image_count, (void **)&orig_dyld_image_count);

	for (int i = 0; i < _dyld_image_count(); i++) {
		printf("image[%d]: %s\n", i, _dyld_get_image_name(i));
	}
	return 0;
}
