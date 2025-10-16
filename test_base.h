#pragma once

// Individual test functions
void test_wasi_heap(void);
void test_buddy(void);
void test_arena(void);
void test_scratch(void);
void test_format(void);
void test_hashtable_int_string(void);
void test_hashtable_string_int(void);
void test_vector_int(void);
void test_vector_int_ptr(void);
void test_string(void);

// Main test runner
void test_base(void);
