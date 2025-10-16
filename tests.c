#include <stdio.h>
#include <test_stdlib.h>
#include <test_base.h>

int main(void) {
    test_stdlib();
    test_base();

    printf("=== All tests passed ===\n");
    return 0;
}
