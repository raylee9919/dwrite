// Copyright (c) 2025 Seong Woo Lee. All rights reserved.
#ifndef LSW_MATH_H
#define LSW_MATH_H

typedef struct V2 V2;
struct V2
{
    F32 x, y;
};

static V2 v2(F32 x, F32 y);
static V2 operator + (V2 a, V2 b);
static F32 px_from_pt(F32 pt);
static V2 px_from_pt(V2 pt);

#endif // LSW_MATH_H
