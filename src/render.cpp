// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

static void
render_quad_px(V2 min, V2 max)
{
    assume(index_count + 6 <= MAX_INDEX_COUNT);
    assume(vertex_count + 4 <= MAX_VERTEX_COUNT);

    vertices[vertex_count++].pos = V2{min.x, max.y};
    vertices[vertex_count++].pos = V2{max.x, max.y};
    vertices[vertex_count++].pos = V2{max.x, min.y};
    vertices[vertex_count++].pos = V2{min.x, min.y};

    indices[index_count++] = 0;
    indices[index_count++] = 1;
    indices[index_count++] = 2;
    indices[index_count++] = 0;
    indices[index_count++] = 2;
    indices[index_count++] = 3;
}
