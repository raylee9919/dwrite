// Copyright (c) 2025 Seong Woo Lee. All rights reserved.
#ifndef LSW_ARENA_H
#define LSW_ARENA_H

// @Note: This may be the simplest form of arena implementation on the planet.

typedef struct Arena Arena;
struct Arena
{
    void *base;
    U64 size;
    U64 used;
};


static Arena arena_alloc(U64 size);
#define arena_push_array(arena, type, count) (type *)arena_push(arena, sizeof(type)*count)
static void *arena_push(Arena *arena, U64 size);
static void arena_clear(Arena *arena);


#endif // LSW_ARENA_H
