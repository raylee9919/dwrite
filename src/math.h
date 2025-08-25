// Copyright (c) 2025 Seong Woo Lee. All rights reserved.
#ifndef LSW_MATH_H
#define LSW_MATH_H

typedef struct V2 V2;
struct V2
{
    F32 x, y;
};

static V2 operator + (V2 a, V2 b);
#endif // LSW_MATH_H
