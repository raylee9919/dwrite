// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

static void
render_quad_px_min_max(V2 min, V2 max)
{
    Renderer &r = renderer;

    assume(r.index_count + 6 <= MAX_INDEX_COUNT);
    assume(r.vertex_count + 4 <= MAX_VERTEX_COUNT);

    U32 start_index = r.vertex_count;

    r.vertices[r.vertex_count].uv    = V2{0,1};
    r.vertices[r.vertex_count++].pos = V2{min.x, min.y};

    r.vertices[r.vertex_count].uv    = V2{0,0};
    r.vertices[r.vertex_count++].pos = V2{min.x, max.y};

    r.vertices[r.vertex_count].uv    = V2{1,0};
    r.vertices[r.vertex_count++].pos = V2{max.x, max.y};

    r.vertices[r.vertex_count].uv    = V2{1,1};
    r.vertices[r.vertex_count++].pos = V2{max.x, min.y};

    r.indices[r.index_count++] = start_index + 0;
    r.indices[r.index_count++] = start_index + 1;
    r.indices[r.index_count++] = start_index + 2;
    r.indices[r.index_count++] = start_index + 0;
    r.indices[r.index_count++] = start_index + 2;
    r.indices[r.index_count++] = start_index + 3;
}
