#ifndef GENERAL_H
#define GENERAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

typedef uint8_t b8;
typedef uint16_t b16;
typedef uint32_t b32;
typedef uint64_t b64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define KILOBYTES(x) (1024*(x))
#define MEGABYTES(x) (1024*(KILOBYTES(x)))
#define GIGABYTES(x) (1024*(MEGABYTES(x)))

#endif // GENERAL_H
