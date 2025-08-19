// Copyright (c) 2025 Seong Woo Lee. All rights reserved.
#ifndef SWL_BASIC_H
#define SWL_BASIC_H

// @Note: Third-Party Includes
#include <stdint.h>
#include <math.h>

// @Note: Base Types
#define global static
#define local_persist static
#define function static
typedef int8_t   S8;
typedef int16_t  S16;
typedef int32_t  S32;
typedef int64_t  S64;
typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef S8       B8;
typedef S16      B16;
typedef S32      B32;
typedef S64      B64;
typedef float    F32;
typedef double   F64;
typedef struct { U64 u64[2]; } U128;


// Note: Macro-Functions
#define assert(exp) do { if (!(exp)) __debugbreak(); } while(0)
#define assume(exp) do { if (!(exp)) __debugbreak(); } while(0)
#define array_count(arr) (sizeof(arr) / sizeof(arr[0]))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define memory_copy(src, dst, bytes) memcpy((void *)dst, (void *)src, bytes)


#endif // SWL_BASIC_H
