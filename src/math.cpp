// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

static V2
v2(F32 x, F32 y)
{
    V2 result = {};
    result.x = x;
    result.y = y;
    return result;
}

static V2
operator + (V2 a, V2 b)
{
    V2 result = {};
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}
