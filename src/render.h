// Copyright (c) 2025 Seong Woo Lee. All rights reserved.
#ifndef LSW_RENDER_H
#define LSW_RENDER_H

/* --------------------------------------
   @Important:
   It's up to backends to convert the coordinate system of ours to theirs.
   Our renderer follows the follwing coordinate system:

   [1] Texture

   (0, 0)        (1, 0)




   (0, 1)        (1, 1)


   [2] px (DIP)

   (0, max_px_y)        (max_px_x, max_px_y)




   (0, 0)               (max_px_x, 0)

   --------------------------------------- */

#define MAX_VERTEX_COUNT 1000
#define MAX_INDEX_COUNT 1000

typedef struct Vertex Vertex;
struct Vertex 
{
    V2 pos;
    V2 uv;
};

typedef struct Renderer Renderer;
struct Renderer
{
    U32 vertex_count = 0;
    Vertex vertices[MAX_VERTEX_COUNT];

    U32 index_count = 0;
    U32 indices[MAX_INDEX_COUNT];
};

static Renderer renderer;


static void render_quad_px_min_max(V2 min, V2 max);
static void render_glyph_px_origin(V2 origin_px);


#endif // LSW_RENDER_H
