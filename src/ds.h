// Copyright (c) 2025 Seong Woo Lee. All rights reserved.
#ifndef LSW_DS_H
#define LSW_DS_H

struct Array_Header
{
    U64 element_size;
    U64 count_cur;
    U64 count_max;
};

#define array_alloc(arena, type, count) (type *)array_alloc_(arena, sizeof(type), count)
static void *
array_alloc_(Arena *arena, U64 element_size, U64 count)
{
    U64 header_size = sizeof(Array_Header);
    Array_Header *header = (Array_Header *)arena_push(arena, element_size*count + sizeof(Array_Header));
    header->element_size = element_size;
    header->count_cur = 0;
    header->count_max = count;

    U8 *base = (U8 *)header + header_size;
    return base;
}

#define array_push(ptr, item) array_push_(ptr, &item, sizeof(item))
static void
array_push_(void *ptr, void *item, U64 item_size)
{
    U64 header_size = sizeof(Array_Header);
    Array_Header *header = (Array_Header *)((U8 *)ptr - header_size);
    assume(header->count_cur < header->count_max);
    U64 offset = header->count_cur*header->element_size;
    U8 *dst = (U8 *)ptr + offset;
    U8 *src = (U8 *)(item);
    for (U32 i = 0; i < item_size; ++i)
    { dst[i] = src[i]; }
    header->count_cur++;
}

#endif // LSW_DS_H
