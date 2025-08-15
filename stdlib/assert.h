#pragma once

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function);

#ifdef NDEBUG
#define assert(condition) ((void)0)
#else
#define assert(condition) ((condition) ? (void)0 : __assert_fail(#condition, __FILE__, __LINE__, __func__))
#endif
