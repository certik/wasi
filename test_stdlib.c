#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <test_stdlib.h>

// Helper function to test string equality
static void test_streq(const char *actual, const char *expected, const char *test_name) {
    size_t i = 0;
    while (actual[i] != '\0' && expected[i] != '\0') {
        if (actual[i] != expected[i]) {
            printf("FAIL: %s - mismatch at position %zu: got '%c', expected '%c'\n",
                   test_name, i, actual[i], expected[i]);
            assert(0);
        }
        i++;
    }
    if (actual[i] != expected[i]) {
        printf("FAIL: %s - length mismatch\n", test_name);
        assert(0);
    }
}

// String function tests
static void test_string_functions(void) {
    printf("## Testing string functions...\n");

    // strlen tests
    assert(strlen("") == 0);
    assert(strlen("a") == 1);
    assert(strlen("hello") == 5);
    assert(strlen("Hello World!") == 12);

    // strcpy tests
    char dest[50];
    strcpy(dest, "");
    test_streq(dest, "", "strcpy empty string");

    strcpy(dest, "test");
    test_streq(dest, "test", "strcpy simple");

    strcpy(dest, "Hello World!");
    test_streq(dest, "Hello World!", "strcpy with spaces");

    // Test strcpy returns dest
    char *ret = strcpy(dest, "return test");
    assert(ret == dest);

    // memcpy tests
    char src[] = "abcdef";
    char dst[10];

    memcpy(dst, src, 0);  // Copy nothing

    memcpy(dst, src, 3);
    assert(dst[0] == 'a');
    assert(dst[1] == 'b');
    assert(dst[2] == 'c');

    memcpy(dst, src, 7);  // Copy including null terminator
    test_streq(dst, "abcdef", "memcpy full string");

    // Test memcpy returns dest
    void *ret_ptr = memcpy(dst, src, 3);
    assert(ret_ptr == dst);

    // Test memcpy with numbers
    int nums_src[] = {1, 2, 3, 4, 5};
    int nums_dst[5];
    memcpy(nums_dst, nums_src, sizeof(nums_src));
    for (int i = 0; i < 5; i++) {
        assert(nums_dst[i] == nums_src[i]);
    }

    printf("String function tests passed\n");
}

// Printf format specifier tests
static void test_printf_formats(void) {
    printf("## Testing printf format specifiers...\n");

    // Basic string and character
    printf("Test %%s: %s\n", "Hello");
    printf("Test %%c: %c\n", 'X');
    printf("Test %%%%: %%\n");

    // Integers - positive, negative, zero
    printf("Test %%d positive: %d\n", 42);
    printf("Test %%d negative: %d\n", -42);
    printf("Test %%d zero: %d\n", 0);
    printf("Test %%d max int: %d\n", 2147483647);
    printf("Test %%d min int: %d\n", -2147483648);

    // Unsigned integers
    printf("Test %%u: %u\n", 42);
    printf("Test %%u zero: %u\n", 0);
    printf("Test %%u large: %u\n", 4294967295u);

    // size_t with %zu
    size_t sz = 12345;
    printf("Test %%zu: %zu\n", sz);
    printf("Test %%zu zero: %zu\n", (size_t)0);

    // Pointers
    int x = 10;
    printf("Test %%p non-null: %p\n", (void*)&x);
    printf("Test %%p null: %p\n", (void*)0);
    printf("Test %%p value: %p\n", (void*)(uintptr_t)0xdeadbeef);

    // NULL string handling
    char *null_str = 0;
    printf("Test %%s NULL: %s\n", null_str);

    // Multiple format specifiers in one call
    int num = 123;
    char *str = "test";
    char ch = 'A';
    unsigned int unum = 456u;
    printf("Multiple: %d %s %c %u\n", num, str, ch, unum);

    // Edge cases
    printf("Empty format test\n");
    printf("%s%s%s\n", "", "", "");

    printf("Printf format tests passed\n");
}

// Assert tests
static void test_assert(void) {
    printf("## Testing assert...\n");

    // These should all pass (not trigger)
    assert(1);
    assert(1 == 1);
    assert(5 > 3);
    assert(10 + 5 == 15);

    int a = 10;
    assert(a == 10);
    assert(a > 0);

    char *str = "test";
    assert(str != 0);
    assert(strlen(str) == 4);

    printf("Assert tests passed (all assertions succeeded)\n");
}

// Main stdlib test function
void test_stdlib(void) {
    printf("=== stdlib tests ===\n");

    test_string_functions();
    printf("\n");

    test_printf_formats();
    printf("\n");

    test_assert();
    printf("\n");

    printf("stdlib tests passed\n\n");
}
