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
typedef union { U32 u32[4]; U64 u64[2]; } U128;

typedef struct Bitmap Bitmap;
struct Bitmap
{
    U32 width;
    U32 height;
    U32 pitch;
    U32 size;
    U8 *data;
};


// @Note: Macro-Functions
#define assert(exp) do { if (!(exp)) __debugbreak(); } while(0)
#define assume(exp) do { if (!(exp)) __debugbreak(); } while(0)
#define array_count(arr) (sizeof(arr) / sizeof(arr[0]))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define memory_copy(src, dst, bytes) memcpy((void *)dst, (void *)src, bytes)
#define quick_sort(ptr, count, each_size, comp) qsort(ptr, count, each_size, comp)
#define offset_of(type, member) (&((((type) *)0)->(member)))

#define KB(value) (   value  * 1024ll)
#define MB(value) (KB(value) * 1024ll)
#define GB(value) (MB(value) * 1024ll)
#define TB(value) (GB(value) * 1024ll)


// @Note: Data Structures
#define dll_append(sentinel, item) {  \
    sentinel->prev->next = item;      \
    item->prev = sentinel->prev;      \
    item->next = sentinel;            \
    sentinel->prev = item;            \
}


#endif // SWL_BASIC_H
