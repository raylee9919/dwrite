// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

struct VS_Input 
{
    float2 position : POS;
    float2 uv       : TEX;
};

struct VS_Output 
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

VS_Output
panel_vs_main(VS_Input input)
{
    VS_Output output;
    output.position = float4(input.position, 0.0f, 1.0f);
    output.uv       = input.uv;
    return output;
}

float4
panel_ps_main(VS_Output input) : SV_Target
{
    float4 result = float4(0.2f, 0.05f, 0.05f, 0.4f);
    return result;
}
