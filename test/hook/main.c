#include <stdio.h>

int add2(int a, int b) {
    return a + b;
}

__attribute__((naked)) int add(int a, int b) {
#ifdef __arm64__
    asm volatile("b _add2");
#elif __x86_64__
    asm volatile("call _add2");
#endif
}

int main() {
    int res = add(1, 2);
    printf("1 + 2 = %d\n", res);
    return 0;
}
