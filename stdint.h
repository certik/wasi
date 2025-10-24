#pragma once

#include <base/base_types.h>

#ifndef INT8_C
#define INT8_C(value) value
#endif

#ifndef UINT8_C
#define UINT8_C(value) value##u
#endif

#ifndef INT16_C
#define INT16_C(value) value
#endif

#ifndef UINT16_C
#define UINT16_C(value) value##u
#endif

#ifndef INT32_C
#define INT32_C(value) value
#endif

#ifndef UINT32_C
#define UINT32_C(value) value##u
#endif

#ifndef INT64_C
#define INT64_C(value) value##ll
#endif

#ifndef UINT64_C
#define UINT64_C(value) value##ull
#endif

#ifndef INTMAX_C
#define INTMAX_C(value) INT64_C(value)
#endif

#ifndef UINTMAX_C
#define UINTMAX_C(value) UINT64_C(value)
#endif

#ifndef INT32_MAX
#define INT32_MAX ((int32_t)0x7FFFFFFF)
#endif

#ifndef UINT32_MAX
#define UINT32_MAX ((uint32_t)0xFFFFFFFFu)
#endif

#ifndef INT64_MAX
#define INT64_MAX ((int64_t)0x7FFFFFFFFFFFFFFFll)
#endif

#ifndef UINT64_MAX
#define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFull)
#endif
