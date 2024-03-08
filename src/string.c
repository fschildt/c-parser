#include "string.h"

#include <string.h>

b32 strings_equal(const char *s1, i32 len1, const char *s2, i32 len2)
{
    if (len1 != len2)
    {
        return false;
    }

    return memcmp(s1, s2, len1) == 0;
}

b32 strings_equal_ref(StringRef str1, StringRef str2)
{
    if (str1.length != str2.length)
    {
        return false;
    }

    return (memcmp(str1.location, str2.location, str1.length) == 0);
}

