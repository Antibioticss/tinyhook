#include <stdio.h>
#include <string.h>

int add(int a, int b) {
	return a + b;
}

int main() {
	printf("Hello, world!\n");
	int res = add(1, 2);
	printf("1 + 2 = %d\n", res);
	return 0;
}
