// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

static Arena
arena_alloc(U64 size)
{
    Arena result = {};
    result.base = malloc(size);
    result.size = size;
    result.used = 0;
    return result;
}

static void *
arena_push(Arena *arena, U64 size)
{
    assume(arena->used + size <= arena->size);

    void *result = (U8 *)arena->base + arena->used;
    arena->used += size;

    // ZII
    for (U64 i = 0; i < size; ++i)
    { ((U8 *)result)[i] = 0; }

    return result;
}

static void
arena_clear(Arena *arena)
{
    if (arena)
    { arena->used = 0; }
}
