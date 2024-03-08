#ifndef STRING_H
#define STRING_H

#include "general.h"

typedef struct {
    const char *location;
    u32 length;
} StringRef;

b32 strings_equal(const char *str1, i32 len1, const char *str2, i32 len2);
b32 strings_equal_ref(StringRef str1, StringRef str2);

#endif // STRING_H
