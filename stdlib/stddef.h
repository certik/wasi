#pragma once

#include <base_types.h>

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

// Re-export types from base_types.h for stdlib compatibility
