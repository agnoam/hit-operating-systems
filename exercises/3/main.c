#include "math_ops.h"
#include <stdio.h>

int main(void) {
    int a = 6, b = 7;
    
    printf("sum(%d, %d) = %d\n", a, b, add(a, b));
    printf("product(%d, %d) = %d\n", a, b, multiply(a, b));
    return 0;
}
