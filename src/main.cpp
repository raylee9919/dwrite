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
#include <d2d1_3.h>

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

//--------------------
// @Note: [.cpp]
#include "math.cpp"
#include "dwrite.cpp"
#include "d3d11.cpp"
#include "render.cpp"

//--------------------
// @Note: Generated hlsl byte code.
#include "shader_vs.h"
#include "shader_ps.h"


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

    // -----------------------------
    // @Note: D3D
    d3d11_init();
    d3d11_create_swapchain_and_framebuffer(hwnd);



    // -----------------------------
    // @Note: D2D
    ID2D1Factory5 *d2d_factory = NULL;
    {
        D2D1_FACTORY_OPTIONS options = {};
        options.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
        win32_assume_hr(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &d2d_factory));
    }

    ID2D1Device4 *d2d_device = NULL;
    win32_assume_hr(d2d_factory->CreateDevice(d3d11.dxgi_device, &d2d_device));

    ID2D1DeviceContext4 *d2d_device_ctx = NULL;
    {
        D2D1_DEVICE_CONTEXT_OPTIONS options = {};
        win32_assume_hr(d2d_device->CreateDeviceContext(options, &d2d_device_ctx));
    }

    ID3D11Texture2D *d2d_backbuffer = NULL;
    assert(d3d11.swapchain);
    d3d11.swapchain->GetBuffer(0, __uuidof(d2d_backbuffer), (void **)&d2d_backbuffer);

    IDXGISurface *dxgi_surface = NULL;
    win32_assume_hr(d2d_backbuffer->QueryInterface(__uuidof(dxgi_surface), (void **)&dxgi_surface));





    // -------------------------------
    // @Note: Init DWrite
    FLOAT pt_per_em = 128.0f; // equivalent to font size.
    FLOAT dip_per_inch = 96.0f;

    IDWriteFactory3 *dwrite_factory = NULL;
    win32_assume_hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(dwrite_factory), (IUnknown **)&dwrite_factory));

    IDWriteGdiInterop *dwrite_gdi_interop = NULL;
    win32_assume_hr(dwrite_factory->GetGdiInterop(&dwrite_gdi_interop));

    IDWriteFontCollection *font_collection = NULL;
    win32_assume_hr(dwrite_factory->GetSystemFontCollection(&font_collection));

    Dwrite_Outer_Hash_Table *atlas_hash_table_out = NULL;

    WCHAR *base_font_family_name = L"Fira Code";
    UINT32 family_index = 0;
    BOOL family_exists = FALSE;
    win32_assume_hr(font_collection->FindFamilyName(base_font_family_name, &family_index, &family_exists));

    // @Note: Since fallback will be performed, finding base family isn't really necessary for our case.
    // But you may want to find if the base font-family exists in the system initially.
    assert(family_exists);

    // @Note: 
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

    B32 is_cleartype = TRUE;
    Dwrite_Glyph_Run *glyph_runs = NULL;

    IDWriteBitmapRenderTarget *dwrite_render_target = NULL;
    U32 atlas_width  = 1024;
    U32 atlas_height = 1024;
    U32 atlas_pitch  = (atlas_width << 2);
    HDC atlas_dc     = NULL;
    {
        win32_assume_hr(dwrite_gdi_interop->CreateBitmapRenderTarget(0/*[optional]HDC*/, atlas_width, atlas_height, &dwrite_render_target));
        atlas_dc = dwrite_render_target->GetMemoryDC();
    }

    // Clear the Render Target
    assert(atlas_dc);
    HGDIOBJ original = SelectObject(atlas_dc, GetStockObject(DC_PEN));
    SetDCPenColor(atlas_dc, RGB(0,0,0));
    SelectObject(atlas_dc, GetStockObject(DC_BRUSH));
    SetDCBrushColor(atlas_dc, RGB(0,0,0));
    Rectangle(atlas_dc, 0, 0, atlas_width, atlas_height);
    SelectObject(atlas_dc, original);

    // Allocate CPU-side memory.
    U64 atlas_size = atlas_pitch*atlas_height;
    U8 *atlas_data = new U8[atlas_size];



    // -----------------------------
    // @Note: Create Vertex Shader
    ID3D11VertexShader *vertex_shader = NULL;
    {
        win32_assume_hr(d3d11.device->CreateVertexShader(g_vs_main, sizeof(g_vs_main), NULL, &vertex_shader));
    }


    // -----------------------------
    // @Note: Create Pixel Shader
    ID3D11PixelShader *pixel_shader = NULL;
    {
        win32_assume_hr(d3d11.device->CreatePixelShader(g_ps_main, sizeof(g_ps_main), NULL, &pixel_shader));
    }



    // ----------------------------
    // @Note: Create Input Layout
    ID3D11InputLayout *input_layout = NULL;
    {
        D3D11_INPUT_ELEMENT_DESC desc[] =
        {
            { "POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        win32_assume_hr(d3d11.device->CreateInputLayout(desc, array_count(desc), g_vs_main, sizeof(g_vs_main), &input_layout));
    }


    //-------------------------------------
    // @Note: RECT
    // Endpoint exclusive.
    // Windows scale option independent.
    // Windows resolution option dependent.
    RECT window_rect = {};
    GetClientRect(hwnd, &window_rect);
    UINT32 window_width  = (window_rect.right - window_rect.left);
    UINT32 window_height = (window_rect.bottom - window_rect.top);

    //-------------------------------------
    // @Note: Create Vertex Buffer
    ID3D11Buffer *vertex_buffer = NULL;
    {
        D3D11_BUFFER_DESC desc = {};
        {
            desc.ByteWidth       = sizeof(renderer.vertices[0])*MAX_VERTEX_COUNT;
            desc.Usage           = D3D11_USAGE_DYNAMIC;
            desc.BindFlags       = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags       = 0;
        }

        D3D11_SUBRESOURCE_DATA subresource = {};
        {
            subresource.pSysMem          = renderer.vertices;
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
            desc.ByteWidth      = sizeof( renderer.indices[0] ) * MAX_INDEX_COUNT;
            desc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags      = 0;
        }

        D3D11_SUBRESOURCE_DATA subresource = {};
        {
            subresource.pSysMem          = renderer.indices;
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
    ID3D11Texture2D *atlas = NULL;
    {
        D3D11_TEXTURE2D_DESC texture_desc = {};
        {
            texture_desc.Width              = atlas_width;
            texture_desc.Height             = atlas_height;
            texture_desc.MipLevels          = 1;
            texture_desc.ArraySize          = 1;
            texture_desc.SampleDesc.Count   = 1;
            texture_desc.Usage              = D3D11_USAGE_DYNAMIC;
            texture_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
            texture_desc.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE;
            texture_desc.MiscFlags          = 0;

            texture_desc.Format = is_cleartype ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8_UNORM; // @Todo: srgb?
        }

        D3D11_SUBRESOURCE_DATA texture_subresource_data = {};
        {
            texture_subresource_data.pSysMem      = atlas_data;
            texture_subresource_data.SysMemPitch  = atlas_pitch;
        }

        d3d11.device->CreateTexture2D(&texture_desc, &texture_subresource_data, &atlas);
    }

    ID3D11ShaderResourceView *texture_view = NULL;
    d3d11.device->CreateShaderResourceView(atlas, NULL, &texture_view);


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

    UINT64 last_counter;
    {
        LARGE_INTEGER li = {};
        QueryPerformanceCounter(&li);
        last_counter = li.QuadPart;
    }

    WCHAR *text = new WCHAR[655356];
    UINT32 text_length = 0;

    text = L"! Hello->World ;";
    text_length = (UINT32)wcslen(text);


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
        UINT64 new_counter;
        {
            LARGE_INTEGER li = {};
            QueryPerformanceCounter(&li);
            new_counter = li.QuadPart;
        }
        DOUBLE dt = (DOUBLE)(new_counter - last_counter) * counter_frequency_inverse;
        last_counter = new_counter;


        // ---------------------------
        // @Note: Update
        renderer.vertex_count = 0;
        renderer.index_count = 0;

        SetWindowTextW(hwnd, text);

        // -----------------------------
        // @Note: Text container.
        V2 container_min_pt     = V2{200,200};
        F32 container_width_pt  = 200.0f;
        F32 container_height_pt = 300.0f;

        // dip
        V2 container_min_px     = V2{px_from_pt(container_min_pt.x), px_from_pt(container_min_pt.y)};
        F32 container_width_px  = px_from_pt(container_width_pt);
        F32 container_height_px = px_from_pt(container_height_pt);


        // @Todo(lsw): (container_dimension_pt, basleine _pt, string) -> (atlas, uv per glyph, series_of_pt_coordinate_per_glyph)
        // 1. Set rules of coordinate system. (O)
        // 2. Tackle series_of_pt_coordinate_per_glyph first.


        // @Note(lsw): Parse text with DWrite.
        if (glyph_runs)
        { arrfree(glyph_runs); }
        glyph_runs = dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, locale, base_font_family_name, pt_per_em, text, text_length);

        // @Note(lsw): Iterate through generated glyph runs and append uvs for according atlas if exists in the 2-level hash table.
        V2 *glyph_uvs = NULL;

        UINT32 run_count = static_cast<UINT32>(arrlenu(glyph_runs));
        for (UINT32 run_idx = 0; run_idx < run_count; ++run_idx)
        {
            Dwrite_Glyph_Run run_wrapper = glyph_runs[run_idx];
            DWRITE_GLYPH_RUN run = run_wrapper.run;

            UINT64 glyph_count = arrlenu(run_wrapper.indices);

            IDWriteFontFace5 *font_face = run_wrapper.font_face;
            if (hmgeti(atlas_hash_table_out, font_face) != -1/*exists*/)
            {
                Dwrite_Inner_Hash_Table **hash_table_in = hmget(atlas_hash_table_out, font_face);
                for (U32 i = 0; i < glyph_count; ++i)
                {
                    U16 glyph_index = run_wrapper.indices[i];
                    if (hmgeti(*hash_table_in, glyph_index) != -1/*exists*/)
                    {
                        V2 uv = hmget(*hash_table_in, glyph_index);
                        arrput(glyph_uvs, uv);
                    }
                    else
                    {
                        V2 uv = atlas_pack();
                        atlas_set_dirty();
                        hmput(*hash_table_in, glyph_index, uv);
                    }
                }
            }
            else
            {
                Dwrite_Inner_Hash_Table **hash_table_in = new Dwrite_Inner_Hash_Table *;
                for (U32 i = 0; i < glyph_count; ++i)
                {
                    U16 glyph_index = run_wrapper.indices[i];
                    V2 uv = atlas_pack();
                    atlas_set_dirty();
                    hmput(*hash_table_in, glyph_index, uv);
                }
                hmput(atlas_hash_table_out, font_face, hash_table_in);
            }

#if 0
            DWRITE_RENDERING_MODE1 rendering_mode = DWRITE_RENDERING_MODE1_NATURAL;
            DWRITE_MEASURING_MODE measuring_mode  = DWRITE_MEASURING_MODE_NATURAL;
            DWRITE_GRID_FIT_MODE grid_fit_mode    = DWRITE_GRID_FIT_MODE_DEFAULT;

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


            // ------------------------
            // @Note: Create texture.
            const DWRITE_TEXTURE_TYPE texture_type = is_cleartype ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;

            RECT bounds = {};
            hr = analysis->GetAlphaTextureBounds(texture_type, &bounds);
            if (FAILED(hr))
            {
                // @Todo: The font doesn't support DWRITE_TEXTURE_CLEARTYPE_3x1.
                // Retry with DWRITE_TEXTURE_ALIASED_1x1.
                assume(! "x");
            }

            U32 bitmap_width = bounds.right - bounds.left;
            U32 bitmap_height = bounds.bottom - bounds.top;
            U32 bitmap_pitch = bitmap_width;

            if (bitmap_width == 0 || bitmap_height == 0)
            {
                // @Todo: Skip bitmap generation if empty.
                assume(! "x");
            }

            UINT32 bitmap_size = bitmap_width * bitmap_height;
            if (is_cleartype)
            {
                bitmap_size <<= 2; 
                bitmap_pitch <<= 2;
            }

            U8 *bitmap_data_24 = new U8[bitmap_size];

            win32_assume_hr(analysis->CreateAlphaTexture(texture_type, &bounds, bitmap_data_24, bitmap_size));

            // -------------------------
            // @Note: RGB to RGBA
            for (UINT32 r = 0; r < bitmap_height; ++r)
            {
                for (UINT32 c = 0; c < bitmap_width; ++c)
                {
                    BYTE *dst = atlas_data + r*atlas_pitch + c*4;
                    BYTE *src = bitmap_data_24 + r*bitmap_width*3 + c*3;
                    *(UINT32 *)dst = *(UINT32 *)src;
                    if (src[0] == 0 && src[1] == 0  && src[2] == 0)
                    { dst[3] = 0x00; }
                    else
                    { dst[3] = 0xff; }
                }
            }


            // -------------------------
            // @Note: Cleanup
            delete [] bitmap_data_24;

            assert(analysis);
            analysis->Release();
#endif
        }

        // -----------------------------------------
        // @Note: Render text per container.
        V2 origin_pt = {0.f,0.f};
        for (U32 run_idx = 0; run_idx < run_count; ++run_idx)
        {
            Dwrite_Glyph_Run run_wrapper = glyph_runs[run_idx];
            DWRITE_GLYPH_RUN run = run_wrapper.run;

            UINT64 glyph_count = arrlenu(run_wrapper.indices);
            for (U32 i = 0; i < glyph_count; ++i)
            {
                FLOAT advance_pt = run_wrapper.advances[i];
                if (origin_pt.x + advance_pt > container_width_pt)
                {
                    origin_pt.x = 0.0f;
                    origin_pt.y -= run_wrapper.vertical_advance_pt;
                }
                render_glyph_px_origin(px_from_pt(origin_pt));
                origin_pt.x += advance_pt;
            }
        }


        // -----------------------
        // @Note: D3D11 Pass

        for (U32 i = 0; i < renderer.vertex_count; ++i)
        {
            renderer.vertices[i].pos.x = (renderer.vertices[i].pos.x * 2.0f / window_width) - 1.0f;
            renderer.vertices[i].pos.y = (renderer.vertices[i].pos.y * 2.0f / window_height) - 1.0f;
        }

        // ---------------------------
        // @Note: Update vertex buffer.
        {
            D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
            d3d11.device_ctx->Map(vertex_buffer, 0/*index # of subresource*/, D3D11_MAP_WRITE_DISCARD, 0/*flags*/, &mapped_subresource);
            memory_copy(renderer.vertices, mapped_subresource.pData, sizeof(renderer.vertices[0])*renderer.vertex_count);
            d3d11.device_ctx->Unmap(vertex_buffer, 0);
        }

        // ---------------------------
        // @Note: Update index buffer.
        {
            D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
            d3d11.device_ctx->Map(index_buffer, 0/*index # of subresource*/, D3D11_MAP_WRITE_DISCARD, 0/*flags*/, &mapped_subresource);
            memory_copy(renderer.indices, mapped_subresource.pData, sizeof(renderer.indices[0])*renderer.index_count);
            d3d11.device_ctx->Unmap(index_buffer, 0);
        }

        // ---------------------------
        // @Note: Update atlas.
        {
            D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
            d3d11.device_ctx->Map(atlas, 0/*index # of subresource*/, D3D11_MAP_WRITE_DISCARD, 0/*flags*/, &mapped_subresource);
            memory_copy(atlas_data, mapped_subresource.pData, sizeof(atlas_data[0])*atlas_pitch*atlas_height);
            d3d11.device_ctx->Unmap(atlas, 0);
        }




        FLOAT background_color[4] = {0.1f, 0.1f, 0.1f,1.0f};
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
            UINT stride = sizeof(renderer.vertices[0]);
            UINT offset = 0;
            d3d11.device_ctx->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
        }
        d3d11.device_ctx->IASetIndexBuffer(index_buffer, DXGI_FORMAT_R32_UINT, 0/*offset*/);
        d3d11.device_ctx->IASetInputLayout(input_layout);
        d3d11.device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        d3d11.device_ctx->VSSetShader(vertex_shader, NULL, 0);

        d3d11.device_ctx->PSSetShader(pixel_shader, NULL, 0);
#if 1
        d3d11.device_ctx->PSSetShaderResources(0, 1, &texture_view);
        d3d11.device_ctx->PSSetSamplers(0, 1, &sampler_state);
#endif

        d3d11.device_ctx->OMSetRenderTargets(1, &d3d11.framebuffer_view, NULL/*Depth-Stencil View*/);
        d3d11.device_ctx->OMSetBlendState(blend_state, NULL, 0xffffffff);


        d3d11.device_ctx->DrawIndexed(renderer.index_count, 0, 0);

        d3d11.swapchain->Present(1, 0);
    }


    return 0;
}
