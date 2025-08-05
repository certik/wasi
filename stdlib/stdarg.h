#pragma once

#include <stdint.h>
#include <stddef.h>

typedef char* va_list;
#define va_start(ap, last) ((ap) = (va_list)&(last) + sizeof(last))
#define va_arg(ap, type) (*(type*)((ap) += sizeof(type), (ap) - sizeof(type)))
#define va_end(ap)

/*
typedef struct {
    char* ptr;
} va_list;

#define va_start(ap, last) ((ap).ptr = (char*)&(last) + sizeof(last))
#define va_arg(ap, type) (*(type*)((ap).ptr += sizeof(type), (ap).ptr - sizeof(type)))
#define va_end(ap) ((void)0)
*/
