#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int main() {
    int res = add(1, 2);
    printf("1 + 2 = %d\n", res);
    return 0;
}
