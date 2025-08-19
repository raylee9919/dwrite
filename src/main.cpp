// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

// @Study:
// COM, emSize
// Can I release base interface once queried advanced version of it?

// @Todo:
// 1. When it comes to rasterization I'd first try and implement it without emoji support,
// because emojis add some complexity that may be distracting initially (to be precisely, it requires a call to TranslateColorGlyphRun)
// 
// 2. Select Hashtable keys

//----------------------------------------------
// @Note: 
// "dwrite_2.h" minimum: Windows 8.1
// "dwrite_3.h" minimum: Windows 10 Build 16299

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <dwrite_3.h> 
#include <d3d11_1.h>
#include <d3dcompiler.h>

#include <stdio.h>
#include <stdlib.h>

//--------------------
// @Note: [.h]
#include "basic.h"
#include "math.h"
#define STB_DS_IMPLEMENTATION
#define STBDS_ASSERT
#include "third_party/stb_ds.h"
#include "dwrite.h"
#include "d3d11.h"
#include "render.h"
#include "shader.h"

//--------------------
// @Note: [.cpp]
#include "math.cpp"
#include "dwrite.cpp"
#include "d3d11.cpp"
#include "render.cpp"


#define win32_assume_hr(hr) assume(SUCCEEDED(hr))


static BOOL g_running = TRUE;


static LRESULT CALLBACK
win32_window_procedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (message)
    {
        case WM_CLOSE:
        case WM_DESTROY: {
            g_running = FALSE;
            PostQuitMessage(0);
        } break;

        default: {
            result = DefWindowProcW(hWnd, message, wParam, lParam);
        }
    }
    return result;
}

int WINAPI
wWinMain(HINSTANCE hinst, HINSTANCE /*hprevinst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/)
{
    HRESULT hr = S_OK;

    // @Note: Place this before creating window.
    win32_assume_hr(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

    WNDCLASSW wcex = {};
    {
        wcex.style              = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
        wcex.lpfnWndProc        = win32_window_procedure;
        wcex.cbClsExtra         = 0;
        wcex.cbWndExtra         = 0;
        wcex.hInstance          = hinst;
        wcex.hIcon              = NULL;
        wcex.hCursor            = LoadCursor(hinst, IDC_ARROW);
        wcex.hbrBackground      = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wcex.lpszMenuName       = NULL;
        wcex.lpszClassName      = L"DirectWriteExampleClass";
    }
    assume(RegisterClassW(&wcex));

    HWND hwnd = CreateWindowExW(0/*style->DWORD*/, wcex.lpszClassName, L"DirectWrite",
                                WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                NULL, NULL, hinst, NULL);
    assume(hwnd);

    // ------------------------------
    // @Note: Query QPC frequency.
    DOUBLE counter_frequency_inverse;
    {
        LARGE_INTEGER li = {};
        QueryPerformanceFrequency(&li);
        counter_frequency_inverse = (1.0 / (DOUBLE)li.QuadPart);
    }


    // @Note: Init DWrite
    IDWriteFactory3 *dwrite_factory = NULL;

    FLOAT const pt_per_em = 70.0f; // equivalent to font size.
    FLOAT const inch_per_em = pt_per_em / 72.0f;
    FLOAT const dip_per_inch = 96.0f;
    FLOAT const dip_per_em = dip_per_inch * inch_per_em;

    const WCHAR *base_font_family_name = L"Fira Code";
    IDWriteFontCollection *font_collection = NULL;
    UINT32 family_index = 0;
    BOOL family_exists = FALSE;

    win32_assume_hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(dwrite_factory), (IUnknown **)&dwrite_factory));
    win32_assume_hr(dwrite_factory->GetSystemFontCollection(&font_collection));
    win32_assume_hr(font_collection->FindFamilyName(base_font_family_name, &family_index, &family_exists));

    // @Note: Since fallback will be performed, finding base family isn't really necessary for our case.
    // But you may want to find if the base font-family exists in the system initially.
    assert(family_exists);

    // @Note: 
    // Use 'IDWriteFontFallback::MapCharacters()' and 'IDWriteTextAnalyzer::GetGlyphs()'.
    // Those are for 'complex glyph shaping' and are necessary for all cases where there isn't a direct 1:1 mapping between characters and glyphs. (e.g., Arabics, Ligatures)
    // 
    // 'IDWriteTextLayout' allows you to map a string and a base font into chunks of string with a resolved font.
    // For example, if there's a emoji in the string, it would automatically fallback from the builtin font to Segoe UI (the emoji font on Windows). 
    IDWriteFontFallback *font_fallback = NULL;
    win32_assume_hr(dwrite_factory->GetSystemFontFallback(&font_fallback));

    IDWriteFontFallback1 *font_fallback1 = NULL;
    win32_assume_hr(font_fallback->QueryInterface(__uuidof(font_fallback1), (void **)&font_fallback1));

    IDWriteTextAnalyzer *text_analyzer = NULL;
    win32_assume_hr(dwrite_factory->CreateTextAnalyzer(&text_analyzer));

    IDWriteTextAnalyzer1 *text_analyzer1 = NULL;
    win32_assume_hr(text_analyzer->QueryInterface(__uuidof(text_analyzer1), (void **)&text_analyzer1));


    WCHAR locale[LOCALE_NAME_MAX_LENGTH]; // LOCALE_NAME_MAX_LENGTH includes a terminating null character.
    if (! GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH))
    { memory_copy(L"en-US", locale, sizeof(L"en-US")); }



    // -----------------------------
    // @Note: Prepare DWrite

    IDWriteRenderingParams *rendering_params = NULL;
    win32_assume_hr(dwrite_factory->CreateRenderingParams(&rendering_params));

    BOOL is_cleartype = TRUE;
    Dwrite_Glyph_Run *glyph_runs = NULL;
    Dwrite_Outer_Hash_Table *outer_hashtable = NULL;

    BYTE *bitmap_data = NULL;
    UINT bitmap_width, bitmap_height, bitmap_pitch;



    // -----------------------------
    // @Note: D3D
    d3d11_init();
    d3d11_create_swapchain_and_framebuffer(hwnd);

#if BUILD_DEBUG
    UINT compile_flags = D3DCOMPILE_DEBUG|D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compile_flags = 0;
#endif


    // -----------------------------
    // @Note: Create Vertex Shader
    ID3DBlob *vs_blob;
    ID3DBlob *vs_error_blob;
    ID3D11VertexShader *vertex_shader;

    if (FAILED(D3DCompile(shader_code, strlen(shader_code)*sizeof(shader_code[0]), "VertexShaderUntitled",
                          NULL/*Defines*/, NULL/*Includes*/, "vs_main",
                          "vs_5_0", compile_flags, 0/*Compile Flags 2*/,
                          &vs_blob, &vs_error_blob)))
    {
        OutputDebugStringA((const char *)vs_error_blob->GetBufferPointer());
        assume(! "x");
    }

    if (FAILED(d3d11.device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), NULL, &vertex_shader)))
    { assume(! "x"); }


    // -----------------------------
    // @Note: Create Pixel Shader
    ID3DBlob *ps_blob;
    ID3DBlob *ps_error_blob;
    ID3D11PixelShader *pixel_shader;

    if (FAILED(D3DCompile(shader_code, strlen(shader_code)*sizeof(shader_code[0]), "PixelShaderUntitled",
                          NULL/*Defines*/, NULL/*Includes*/, "ps_main",
                          "ps_5_0", compile_flags, 0/*Compile Flags 2*/,
                          &ps_blob, &ps_error_blob)))
    {
        OutputDebugStringA((const char *)ps_error_blob->GetBufferPointer());
        assume(! "x");
    }

    if (FAILED(d3d11.device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), NULL, &pixel_shader)))
    { assume(! "x"); }

    ps_blob->Release();


    // ----------------------------
    // @Note: Create Input Layout
    ID3D11InputLayout* input_layout;
    D3D11_INPUT_ELEMENT_DESC input_element_desc[] =
    {
        { "POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(d3d11.device->CreateInputLayout(input_element_desc, array_count(input_element_desc), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &input_layout)))
    { assume(! "x"); }

    vs_blob->Release();


    //-------------------------------------
    // @Note: RECT
    // Endpoint exclusive.
    // Windows scale option independent.
    // Windows resolution option dependent.
    RECT window_rect = {};
    GetClientRect(hwnd, &window_rect);
    UINT32 window_width  = (window_rect.right - window_rect.left);
    UINT32 window_height = (window_rect.bottom - window_rect.top);

    FLOAT sw = 0.5f; //(FLOAT)bitmap_width / (FLOAT)window_width;
    FLOAT sh = 0.5f; //(FLOAT)bitmap_height / (FLOAT)window_height;

    // -----------------------------
    // @Note: Create Vertex Buffer
    ID3D11Buffer *vertex_buffer = NULL;
    {
        D3D11_BUFFER_DESC desc = {};
        {
            desc.ByteWidth       = sizeof(vertices[0])*MAX_VERTEX_COUNT;
            desc.Usage           = D3D11_USAGE_DYNAMIC;
            desc.BindFlags       = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags       = 0;
        }

        D3D11_SUBRESOURCE_DATA subresource = {};
        {
            subresource.pSysMem          = vertices;
            subresource.SysMemPitch      = 0;
            subresource.SysMemSlicePitch = 0;
        }

        win32_assume_hr(d3d11.device->CreateBuffer(&desc, &subresource, &vertex_buffer));
    }



    // -----------------------------
    // @Note: Create Index Buffer
    ID3D11Buffer *index_buffer = NULL;
    {
        D3D11_BUFFER_DESC desc = {};
        {
            desc.Usage          = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth      = sizeof( indices[0] ) * MAX_INDEX_COUNT;
            desc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags      = 0;
        }

        D3D11_SUBRESOURCE_DATA subresource = {};
        {
            subresource.pSysMem          = indices;
            subresource.SysMemPitch      = 0;
            subresource.SysMemSlicePitch = 0;
        }

        win32_assume_hr(d3d11.device->CreateBuffer(&desc, &subresource, &index_buffer));
    }



    // ------------------------------
    // @Note: Create Sampler State
    D3D11_SAMPLER_DESC sampler_desc = {};
    {
        sampler_desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampler_desc.AddressU       = D3D11_TEXTURE_ADDRESS_BORDER;
        sampler_desc.AddressV       = D3D11_TEXTURE_ADDRESS_BORDER;
        sampler_desc.AddressW       = D3D11_TEXTURE_ADDRESS_BORDER;
        sampler_desc.BorderColor[0] = 1.0f;
        sampler_desc.BorderColor[1] = 1.0f;
        sampler_desc.BorderColor[2] = 1.0f;
        sampler_desc.BorderColor[3] = 1.0f;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    }

    ID3D11SamplerState* sampler_state;
    d3d11.device->CreateSamplerState(&sampler_desc, &sampler_state);



    // ------------------------------
    // @Note: Create Texture

#if 0
    ID3D11Texture2D *texture = NULL;
    {
        D3D11_TEXTURE2D_DESC texture_desc = {};
        {
            texture_desc.Width              = bitmap_width;
            texture_desc.Height             = bitmap_height;
            texture_desc.MipLevels          = 1;
            texture_desc.ArraySize          = 1;
            texture_desc.SampleDesc.Count   = 1;
            texture_desc.Usage              = D3D11_USAGE_IMMUTABLE;
            texture_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

            texture_desc.Format = is_cleartype ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8_UNORM; // @Todo: srgb?
        }

        D3D11_SUBRESOURCE_DATA texture_subresource_data = {};
        {
            texture_subresource_data.pSysMem      = bitmap_data;
            texture_subresource_data.SysMemPitch  = bitmap_pitch;
        }

        d3d11.device->CreateTexture2D(&texture_desc, &texture_subresource_data, &texture);
    }

    ID3D11ShaderResourceView *texture_view = NULL;
    d3d11.device->CreateShaderResourceView(texture, NULL, &texture_view);
#endif

    // ------------------------------
    // @Note: Create Blend State

    D3D11_BLEND_DESC blend_desc = {};
    {
        D3D11_RENDER_TARGET_BLEND_DESC target_blend_desc = {};
        {
            target_blend_desc.BlendEnable           = TRUE;
            target_blend_desc.SrcBlend              = D3D11_BLEND_ONE;
            target_blend_desc.DestBlend             = D3D11_BLEND_INV_SRC1_COLOR;
            target_blend_desc.BlendOp               = D3D11_BLEND_OP_ADD;
            target_blend_desc.SrcBlendAlpha         = D3D11_BLEND_ONE;
            target_blend_desc.DestBlendAlpha        = D3D11_BLEND_INV_SRC1_ALPHA;
            target_blend_desc.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
            target_blend_desc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }
        blend_desc.AlphaToCoverageEnable  = FALSE;
        blend_desc.IndependentBlendEnable = FALSE;
        blend_desc.RenderTarget[0]        = target_blend_desc;
    }

    ID3D11BlendState *blend_state = NULL;
    hr = d3d11.device->CreateBlendState(&blend_desc, &blend_state);
    assume(SUCCEEDED(hr));

    FLOAT last_counter;
    {
        LARGE_INTEGER li = {};
        QueryPerformanceCounter(&li);
        last_counter = li.QuadPart;
    }

    // ------------------------------
    // @Note: Main Loop
    while (g_running)
    {
        for (MSG msg; PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE);)
        {
            switch (msg.message)
            {
                case WM_QUIT: {
                    g_running = FALSE;
                } break;

                default: {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                } break;
            }
        }

        // ---------------------------
        // @Note: Query new dt
        DOUBLE new_counter;
        {
            LARGE_INTEGER li = {};
            QueryPerformanceCounter(&li);
            new_counter = (DOUBLE)li.QuadPart;
        }
        DOUBLE dt = (DOUBLE)(new_counter - last_counter) * counter_frequency_inverse;
        last_counter = new_counter;


        // ---------------------------
        // @Note: Update
        vertex_count = 0;
        index_count = 0;

        // ---------------------------
        // @Note: Parse text with DWrite.

        WCHAR *text = L"! Hello->World ;";

        UINT32 text_length = wcslen(text);

        if (glyph_runs)
        { arrfree(glyph_runs); }
        Dwrite_Glyph_Run *glyph_runs = dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, locale, 
                                                                 base_font_family_name, pt_per_em, text, text_length);

        if (bitmap_data)
        { delete [] bitmap_data; }

        F32 container_width_pt  = 100.0f;
        F32 container_height_pt = 100.0f;

        // dip
        F32 container_width_px  = container_width_pt  / 72.0f * 96.0f;
        F32 container_height_px = container_height_pt / 72.0f * 96.0f;


        //render_quad_px(v2(50, 50), v2(50 + container_width_px, 50 + container_height_px));
        render_quad_px(v2(-0.5f, -0.5f), v2(0.5f, 0.5f));
        



        UINT32 run_count = arrlenu(glyph_runs);
        for (UINT32 i = 0; i < run_count; ++i)
        {
            Dwrite_Glyph_Run glyph_run = glyph_runs[i];
            DWRITE_GLYPH_RUN run = glyph_run.run;

            DWRITE_RENDERING_MODE1 rendering_mode = DWRITE_RENDERING_MODE1_NATURAL;
            DWRITE_MEASURING_MODE measuring_mode = DWRITE_MEASURING_MODE_NATURAL;
            DWRITE_GRID_FIT_MODE grid_fit_mode = DWRITE_GRID_FIT_MODE_DEFAULT;

            win32_assume_hr(glyph_run.font_face->GetRecommendedRenderingMode(run.fontEmSize,
                                                                             dip_per_inch, dip_per_inch,
                                                                             NULL, // transform
                                                                             run.isSideways,
                                                                             DWRITE_OUTLINE_THRESHOLD_ANTIALIASED,
                                                                             measuring_mode,
                                                                             rendering_params,
                                                                             &rendering_mode,
                                                                             &grid_fit_mode));

            // @Note: CreateGlyphRunAnalysis() doesn't support DWRITE_RENDERING_MODE_OUTLINE.
            // We won't bother big glyphs. (many hundreds of pt)
            if (rendering_mode == DWRITE_RENDERING_MODE1_OUTLINE)
            { rendering_mode = DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC; }

            IDWriteGlyphRunAnalysis *analysis = NULL;
            hr = dwrite_factory->CreateGlyphRunAnalysis(&run,
                                                        NULL, // transform
                                                        rendering_mode,
                                                        measuring_mode,
                                                        grid_fit_mode,
                                                        is_cleartype ? DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE : DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE,
                                                        0.0f, // baselineOriginX
                                                        0.0f, // baselineOriginY
                                                        &analysis);

            const DWRITE_TEXTURE_TYPE texture_type = is_cleartype ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;

            // @Note: As long as you don't Release() font face, Windows will
            // internally return the same address for the same font face.
            IDWriteFontFace5 *font_face = glyph_run.font_face; // Outer-key.
            if (hmgeti(outer_hashtable, font_face) != -1)
            {
                Dwrite_Inner_Hash_Table **inner_hash_table = hmget(outer_hashtable, font_face);

                for (int i = 0; i < run.glyphCount; ++i)
                {
                    UINT16 glyph_index = glyph_run.indices[i]; // Inner-key.

                    if (hmgeti(*inner_hash_table, font_face) != -1)
                    {
                        // @Todo: return uv
                    }
                    else
                    {
                        hmput(*inner_hash_table, glyph_index, 1219); // @Todo: UV value
                    }
                }
            }
            else
            {
                Dwrite_Inner_Hash_Table **inner_hash_table = new Dwrite_Inner_Hash_Table *;
                *inner_hash_table = NULL;

                hmput(outer_hashtable, font_face, inner_hash_table);

                for (int i = 0; i < run.glyphCount; ++i)
                {
                    UINT16 glyph_index = glyph_run.indices[i]; // Inner-key.
                    hmput(*inner_hash_table, glyph_index, 1219); // @Todo: UV value
                }
            }



            RECT bounds = {};
            hr = analysis->GetAlphaTextureBounds(texture_type, &bounds);
            if (FAILED(hr))
            {
                // @Todo: The font doesn't support DWRITE_TEXTURE_CLEARTYPE_3x1.
                // Retry with DWRITE_TEXTURE_ALIASED_1x1.
                assume(! "x");
            }

            bitmap_width = bounds.right - bounds.left;
            bitmap_height = bounds.bottom - bounds.top;
            bitmap_pitch = bitmap_width;

            if (bitmap_width == 0 || bitmap_height == 0)
            {
                // @Todo: Skip bitmap generation if empty.
                assume(! "x");
            }

            UINT64 bitmap_size = bitmap_width * bitmap_height;
            if (is_cleartype)
            {
                bitmap_size <<= 2; 
                bitmap_pitch <<= 2;
            }

            BYTE *bitmap_data_24 = new BYTE[bitmap_size];
            bitmap_data = new BYTE[bitmap_size];

            win32_assume_hr(analysis->CreateAlphaTexture(texture_type, &bounds, bitmap_data_24, bitmap_size));

            // -------------------------
            // @Note: RGB to RGBA
            for (UINT32 r = 0; r < bitmap_height; ++r)
            {
                for (UINT32 c = 0; c < bitmap_width; ++c)
                {
                    BYTE *dst = bitmap_data + r*bitmap_pitch + c*4;
                    BYTE *src = bitmap_data_24 + r*bitmap_width*3 + c*3;
                    *(UINT32 *)dst = *(UINT32 *)src;
                    dst[3] = 0xff;
                }
            }


            // -------------------------
            // @Note: Cleanup
            delete [] bitmap_data_24;

            assert(analysis);
            analysis->Release();
        }



        // ---------------------------
        // @Note: Update vertex buffer.
        {
            D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
            d3d11.device_ctx->Map(vertex_buffer, 0/*index # of subresource*/, D3D11_MAP_WRITE_DISCARD, 0/*flags*/, &mapped_subresource);
            memory_copy(vertices, mapped_subresource.pData, sizeof(vertices[0])*vertex_count);
            d3d11.device_ctx->Unmap(vertex_buffer, 0);
        }

        // ---------------------------
        // @Note: Update index buffer.
        {
            D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
            d3d11.device_ctx->Map(index_buffer, 0/*index # of subresource*/, D3D11_MAP_WRITE_DISCARD, 0/*flags*/, &mapped_subresource);
            memory_copy(indices, mapped_subresource.pData, sizeof(indices[0])*index_count);
            d3d11.device_ctx->Unmap(index_buffer, 0);
        }



        // -----------------------
        // @Note: Draw
        FLOAT background_color[4] = { 0.5f, 0.2f, 0.2f, 1.0f };
        d3d11.device_ctx->ClearRenderTargetView(d3d11.framebuffer_view, background_color);

        D3D11_VIEWPORT viewport = {};
        {
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width    = (FLOAT)(window_width);
            viewport.Height   = (FLOAT)(window_height);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
        }
        d3d11.device_ctx->RSSetViewports(1, &viewport);

        {
            UINT stride = sizeof(vertices[0]);
            UINT offset = 0;
            d3d11.device_ctx->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
        }
        d3d11.device_ctx->IASetIndexBuffer(index_buffer, DXGI_FORMAT_R32_UINT, 0/*offset*/);
        d3d11.device_ctx->IASetInputLayout(input_layout);
        d3d11.device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        d3d11.device_ctx->VSSetShader(vertex_shader, NULL, 0);

        d3d11.device_ctx->PSSetShader(pixel_shader, NULL, 0);
#if 0
        d3d11.device_ctx->PSSetShaderResources(0, 1, &texture_view);
        d3d11.device_ctx->PSSetSamplers(0, 1, &sampler_state);
#endif

        d3d11.device_ctx->OMSetRenderTargets(1, &d3d11.framebuffer_view, NULL/*Depth-Stencil View*/);
        d3d11.device_ctx->OMSetBlendState(blend_state, NULL, 0xffffffff);

        d3d11.device_ctx->DrawIndexed(arrlenu(indices), 0, 0);

        d3d11.swapchain->Present(1, 0);
    }


    return 0;
}
