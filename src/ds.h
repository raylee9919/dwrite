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

// @Note: List/Append
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

// @Note: List/Prepend
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

// @Note: List/Iterate
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
typedef U16 Hash_Table_Flags;
enum
{
    HT_ENTRY_FILLED    = (1<<0),
    HT_ENTRY_TOMBSTONE = (1<<1),
};

typedef struct Hash_Table_Entry Hash_Table_Entry;
struct Hash_Table_Entry
{
    Hash_Table_Flags flags;
    U8 key_val[];
};

function Hash_Table_Entry *
ht_entries_init(Arena *arena, U64 total_size)
{
    Hash_Table_Entry *result = (Hash_Table_Entry *)arena_push(arena, total_size);
    return result;
}

#define HT_ENTRY_COUNT 1024
static_assert(is_power_of_two(HT_ENTRY_COUNT)); // @Note: Since we're using i*i as a probing function.

#define Hash_Table(arena_init, key_type, val_type) struct { \
    Arena *arena = arena_init; \
    U64 entry_count = HT_ENTRY_COUNT; \
    union { \
        Hash_Table_Entry *entries = ht_entries_init(arena_init, (sizeof(Hash_Table_Entry)+sizeof(key_type)+sizeof(val_type))*HT_ENTRY_COUNT); \
        key_type *key_payload; \
        val_type *val_payload; \
    }; \
}

function U64
hash(U8 *data, U64 length)
{
    // @Todo: Better hash function.
    int result = 0;
    for (U64 i = 0; i < length; ++i)
    {
        result += data[i];
    }
    return result;
}

function U64
ht_probing_function(U64 i)
{
    // @Todo: Better probing function.
    return i*i;
}

function Hash_Table_Entry *
ht_search_empty_entry(Hash_Table_Entry *entries, U64 entry_count, U64 entry_size, U64 home_position)
{
    for (U64 i = 0; i < entry_count; ++i)
    {
        U64 probe = ht_probing_function(i); 
        U64 index = (home_position + probe) % entry_count;
        Hash_Table_Entry *entry = (Hash_Table_Entry *)((U8 *)entries + index*entry_size);
        if (!(entry->flags & HT_ENTRY_FILLED)) // empty
        {
            return entry;
        }
    } 
    return NULL;
}

function Hash_Table_Entry *
ht_search(Hash_Table_Entry *entries, U64 entry_count, U64 entry_size, U64 home_position, U8 *key, U64 length)
{
    for (U64 i = 0; i < entry_count; ++i)
    {
        U64 probe = ht_probing_function(i); 
        U64 index = (home_position + probe) % entry_count;
        Hash_Table_Entry *entry = (Hash_Table_Entry *)((U8 *)entries + index*entry_size);
        if (!(entry->flags & HT_ENTRY_TOMBSTONE))
        {
            if (entry->flags & HT_ENTRY_FILLED)
            {
                for (U64 c = 0; c < length; ++c)
                {
                    if (string_equal(entry->key_val, key, length))
                    {
                        return entry;
                    }
                }
            }
            else
            {
                return NULL;
            }
        }
    } 

    assume(! "invalid code path.");
}

#define ht_insert(t, key, val) \
{ \
    decltype(key) _key = (key); \
    U64 hashed = hash((U8 *)(&_key), sizeof(decltype(key))); \
    U64 home_position = hashed % ((t)->entry_count); \
    Hash_Table_Entry *entry = ht_search_empty_entry((t)->entries, (t)->entry_count, sizeof(Hash_Table_Entry)+sizeof(*(t)->key_payload)+sizeof(*(t)->val_payload), home_position); \
    assume(entry); \
    Hash_Table_Entry *header = (Hash_Table_Entry *)entry; \
    header->flags |= HT_ENTRY_FILLED; \
    decltype((t)->key_payload) key_ptr = (decltype((t)->key_payload))(entry + sizeof(Hash_Table_Entry)); \
    decltype((t)->val_payload) val_ptr = (decltype((t)->val_payload))((U8 *)key_ptr + sizeof(*(t)->key_payload)); \
    *key_ptr = key; \
    *val_ptr = val; \
}

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
