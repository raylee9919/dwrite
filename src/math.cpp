// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

static V2
operator + (V2 a, V2 b)
{
    V2 result = {};
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

static F32 
px_from_pt(F32 pt)
{
    return pt * 1.333333f;
}

static V2
px_from_pt(V2 pt)
{
    return {pt.x*1.333333f, pt.y*1.333333f};
}
