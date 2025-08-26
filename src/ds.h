// Copyright (c) 2025 Seong Woo Lee. All rights reserved.
#ifndef LSW_DS_H
#define LSW_DS_H

// ------------------------------------- 
// @Note: List

typedef struct List_Node List_Node;
struct List_Node
{
    List_Node *next;
    List_Node *prev;
    U8 data[]; // @Todo: Alignemnt.
};

function List_Node *
list_sentinel_init(Arena *arena, U64 size)
{
    List_Node *result = (List_Node *)arena_push(arena, size);
    result->next = result;
    result->prev = result;
    return result;
}

#define List(arena_init, type) struct {                                             \
    Arena *arena = arena_init;                                                      \
    union {                                                                         \
        List_Node *sentinel = list_sentinel_init(arena_init, sizeof(List_Node));    \
        type *payload;                                                              \
    };                                                                              \
}

// @Note: Append
#define list_alloc_back(l) \
    ((decltype((l)->payload))_list_alloc_back((l)->arena, (l)->sentinel, sizeof(*(l)->payload)))

#define list_append(list, item) \
    *list_alloc_back(list) = item

function void *
_list_alloc_back(Arena *arena, List_Node *sentinel, U64 data_size)
{
    List_Node *node = (List_Node *)arena_push(arena, sizeof(*node) + data_size);
    assume(node);

    node->prev = sentinel->prev;
    node->next = sentinel;
    sentinel->prev->next = node;
    sentinel->prev = node;

    return node->data;
}

// @Note: Prepend
#define list_alloc_front(l) \
    ((decltype((l)->payload))_list_alloc_front((l)->arena, (l)->sentinel, sizeof(*(l)->payload)))

#define list_prepend(list, item) \
    *list_alloc_front(list) = item

function void *
_list_alloc_front(Arena *arena, List_Node *sentinel, U64 data_size)
{
    List_Node *node = (List_Node *)arena_push(arena, sizeof(*node) + data_size);
    assume(node);

    node->prev = sentinel;
    node->next = sentinel->next;
    sentinel->next->prev = node;
    sentinel->next = node;

    return node->data;
}

// @Note: Iterate
#define list_first_data(l) ( ( decltype((l)->payload) )((l)->sentinel->next != (l)->sentinel ? (l)->sentinel->next->data : NULL) ) 
#define list_for(it, l) for ( decltype((l)->payload) it = list_first_data(l); \
                              it != NULL; \
                              it = (decltype((l)->payload))list_next_data((l)->sentinel, it) )

function void *
list_next_data(List_Node *sentinel, void *data)
{
    U64 offset = offsetof(List_Node, data);
    List_Node *node = (List_Node *)((U8 *)data - offset);
    node = node->next;
    void *result = node != sentinel ? node->data : NULL;
    return result;
}


// ------------------------------------- 
// @Note: Hash_Table

#define Hash_Table(type) Hash_Table(arena_init, type) struct {  \
    Arena *arena = arena_init;                                  \
    union {                                                     \
        type *payload;                                          \
    };                                                          \
};


// ------------------------------------- 
// @Note: Array
typedef struct Array_Header Array_Header;
struct Array_Header
{
    U64 element_size;
    U64 count_cur;
    U64 count_max;
};

#define array_alloc(arena, type, count) (type *)array_alloc_(arena, sizeof(type), count)
function void *
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
function void
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
