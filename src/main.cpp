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

typedef struct Binpack Binpack;
struct Binpack
{
    Binpack *prev;
    Binpack *next;
    B32 occupied;
    U32 x, y, w, h;
};

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



    // -------------------------------
    // @Note: Init DWrite
    FLOAT base_font_pt_per_em = 40.0f; // equivalent to font size.
    FLOAT dip_per_inch = 96.0f;

    IDWriteFactory3 *dwrite_factory = NULL;
    win32_assume_hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(dwrite_factory), (IUnknown **)&dwrite_factory));

    IDWriteFontCollection *font_collection = NULL;
    win32_assume_hr(dwrite_factory->GetSystemFontCollection(&font_collection));

    Dwrite_Outer_Hash_Table *atlas_hash_table_out = NULL;

    //WCHAR *base_font_family_name = L"Fira Code";
    WCHAR *base_font_family_name = L"Consolas";
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
    DWRITE_TEXTURE_TYPE texture_type = is_cleartype ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;
    Dwrite_Glyph_Run *glyph_runs = NULL;

    U32 atlas_width  = 1024;
    U32 atlas_height = 1024;
    U32 atlas_pitch  = (atlas_width << 2);
    U64 atlas_size = atlas_pitch*atlas_height;
    U8 *atlas_data = new U8[atlas_size];
    Binpack *atlas_partition_sentinel = new Binpack;
    {
        Binpack *head = new Binpack;
        {
            head->prev = atlas_partition_sentinel;
            head->next = atlas_partition_sentinel;
            head->occupied = false;
            head->x = 0;
            head->y = 0;
            head->w = atlas_width;
            head->h = atlas_height;
        }
        atlas_partition_sentinel->prev = head;
        atlas_partition_sentinel->next = head;
    }



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

    text = L"!Hello->World;";
    text_length = (U32)wcslen(text);

    // @Note: Iterate through generated glyph runs and append uvs in the atlas if exists in the 2-level hash table.
    Glyph_Cel *glyph_cels = NULL;

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
        V2 container_dim_px = {container_width_px, container_height_px};


        // @Todo(lsw): (container_dimension_pt, basleine _pt, string) -> (atlas, uv per glyph, series_of_pt_coordinate_per_glyph)
        // 1. Set rules of coordinate system. (O)
        // 2. Tackle series_of_pt_coordinate_per_glyph first.


        if (glyph_runs)
        { arrfree(glyph_runs); }
        glyph_runs = dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, locale, base_font_family_name, base_font_pt_per_em, text, text_length);

        if (glyph_cels)
        { arrsetlen(glyph_cels, 0); }

        U64 run_count = arrlenu(glyph_runs);
        for (U32 run_idx = 0; run_idx < run_count; ++run_idx)
        {
            Dwrite_Glyph_Run run_wrapper = glyph_runs[run_idx];
            DWRITE_GLYPH_RUN run = run_wrapper.run;

            UINT64 glyph_count = arrlenu(run_wrapper.indices);

            IDWriteFontFace5 *font_face = run_wrapper.font_face;

            // @Note(lsw): Create rendering mode of a font face.
            DWRITE_RENDERING_MODE1 rendering_mode = DWRITE_RENDERING_MODE1_NATURAL;
            DWRITE_MEASURING_MODE measuring_mode  = DWRITE_MEASURING_MODE_NATURAL;
            DWRITE_GRID_FIT_MODE grid_fit_mode    = DWRITE_GRID_FIT_MODE_DEFAULT;

            win32_assume_hr(font_face->GetRecommendedRenderingMode(run.fontEmSize,
                                                                   dip_per_inch, dip_per_inch,
                                                                   NULL, // transform
                                                                   run.isSideways,
                                                                   DWRITE_OUTLINE_THRESHOLD_ANTIALIASED,
                                                                   measuring_mode,
                                                                   rendering_params,
                                                                   &rendering_mode,
                                                                   &grid_fit_mode));


            // Look into 2-tier hash table.
            if (hmgeti(atlas_hash_table_out, font_face) != -1/*exists*/)
            {
                Dwrite_Inner_Hash_Table **hash_table_in = hmget(atlas_hash_table_out, font_face);

                // @Todo(lsw): Cleanup.
                for (U32 i = 0; i < glyph_count; ++i)
                {
                    U16 glyph_index = run_wrapper.indices[i];

                    if (hmgeti(*hash_table_in, glyph_index) == -1) // glyph index doesn't exist in the inner-table
                    {
                        // Get single glyph's metrics.
                        DWRITE_GLYPH_METRICS metrics = {};
                        win32_assume_hr(font_face->GetDesignGlyphMetrics(&glyph_index, 1, &metrics, run.isSideways));

                        // Calc blackbox of a glyph.
                        assume(run_wrapper.design_units_per_em != 0);
                        FLOAT bb_width_em = (FLOAT)(metrics.advanceWidth - metrics.leftSideBearing - metrics.rightSideBearing) / run_wrapper.design_units_per_em;
                        FLOAT bb_height_em = (FLOAT)(metrics.advanceHeight - metrics.topSideBearing - metrics.bottomSideBearing) / run_wrapper.design_units_per_em;
                        FLOAT pt_per_em = run.fontEmSize;
                        FLOAT bb_width_pt = bb_width_em * pt_per_em;
                        FLOAT bb_height_pt = bb_height_em * pt_per_em;

                        // CreateGlyphRunAnalysis() doesn't support DWRITE_RENDERING_MODE_OUTLINE.
                        // We won't bother big glyphs. (many hundreds of pt)
                        if (rendering_mode == DWRITE_RENDERING_MODE1_OUTLINE)
                        { rendering_mode = DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC; }

                        DWRITE_GLYPH_RUN single_glyph_run = {};
                        {
                            single_glyph_run.fontFace      = font_face;
                            single_glyph_run.fontEmSize    = run.fontEmSize;
                            single_glyph_run.glyphCount    = 1;
                            single_glyph_run.glyphIndices  = &glyph_index;
                            single_glyph_run.glyphAdvances = NULL;
                            single_glyph_run.glyphOffsets  = NULL;
                            single_glyph_run.isSideways    = run.isSideways;
                            single_glyph_run.bidiLevel     = run.bidiLevel;
                        }

                        IDWriteGlyphRunAnalysis *analysis = NULL;
                        win32_assume_hr(dwrite_factory->CreateGlyphRunAnalysis(&single_glyph_run,
                                                                               NULL, // transform
                                                                               rendering_mode,
                                                                               measuring_mode,
                                                                               grid_fit_mode,
                                                                               is_cleartype ? DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE : DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE,
                                                                               0.0f, // baselineOriginX
                                                                               0.0f, // baselineOriginY
                                                                               &analysis));

                        RECT bounds = {};
                        hr = analysis->GetAlphaTextureBounds(texture_type, &bounds);
                        if (FAILED(hr))
                        {
                            // @Todo: The font doesn't support DWRITE_TEXTURE_CLEARTYPE_3x1.
                            // Retry with DWRITE_TEXTURE_ALIASED_1x1.
                            assume(! "x");
                        }

                        U32 bitmap_width  = bounds.right - bounds.left;
                        U32 bitmap_height = bounds.bottom - bounds.top;
                        U32 bitmap_pitch  = is_cleartype ?  bitmap_width << 2 : bitmap_width;
                        U32 bitmap_size   = bitmap_pitch * bitmap_height;

                        if (bitmap_width == 0 || bitmap_height == 0)
                        {
                            // @Todo: Skip bitmap generation if empty.
                            assume(! "x");
                        }
                        else
                        {
                            U8 *bitmap_data_rgb = new U8[bitmap_size];
                            win32_assume_hr(analysis->CreateAlphaTexture(texture_type, &bounds, bitmap_data_rgb, bitmap_size));

                            B32 fit = false;
                            U32 x1, y1, x2, y2;

                            F32 width_px, height_px;

                            for (Binpack *partition = atlas_partition_sentinel->next;
                                 partition != atlas_partition_sentinel;
                                 partition = partition->next)
                            {
                                if (!partition->occupied)
                                {
                                    U32 w1 = partition->w;
                                    U32 h1 = partition->h;
                                    U32 w2 = bitmap_width;
                                    U32 h2 = bitmap_height;
                                    width_px = (F32)bitmap_width;
                                    height_px = (F32)bitmap_height;

                                    if (w1 >= w2 && h1 >= h2)
                                    {
                                        x1 = partition->x;
                                        y1 = partition->y;
                                        x2 = x1 + w2;
                                        y2 = y1 + h2;
                                        fit = true;

                                        U32 dx[3] = {w2, 0, w2};
                                        U32 dy[3] = {0, h2, h2};
                                        U32 nw[3] = {w1-w2, w2, w1-w2};
                                        U32 nh[3] = {h2, h1-h2, h1-h2};

                                        for (U32 npi = 0; npi < 3; ++npi)
                                        {
                                            Binpack *new_partition = new Binpack;
                                            new_partition->occupied = false;
                                            new_partition->x = x1 + dx[npi];
                                            new_partition->y = y1 + dy[npi];
                                            new_partition->w = nw[npi];
                                            new_partition->h = nh[npi];
                                            dll_append(atlas_partition_sentinel, new_partition);
                                        }

                                        partition->occupied = true;
                                        partition->w = w2;
                                        partition->h = h2;

                                        break;
                                    }
                                }
                            }

                            if (fit)
                            {
                                // RGB to RGBA
                                for (UINT32 r = 0; r < bitmap_height; ++r)
                                {
                                    for (UINT32 c = 0; c < bitmap_width; ++c)
                                    {
                                        BYTE *dst = atlas_data + (y1+r)*atlas_pitch + (x1+c)*4;
                                        BYTE *src = bitmap_data_rgb + r*bitmap_width*3 + c*3;
                                        *(UINT32 *)dst = *(UINT32 *)src;
                                        if (src[0] == 0 && src[1] == 0  && src[2] == 0)
                                        { dst[3] = 0x00; }
                                        else
                                        { dst[3] = 0xff; }
                                    }
                                }
                            }
                            else
                            {
                                assume(! "x");
                            }

                            delete [] bitmap_data_rgb;

                            Glyph_Cel cel = {};
                            {
                                cel.uv_min = {(F32)x1 / (F32)atlas_width, (F32)y1 / (F32)atlas_height};
                                cel.uv_max = {(F32)x2 / (F32)atlas_width, (F32)y2 / (F32)atlas_height};
                                cel.width_px = width_px;
                                cel.height_px = height_px;
                                cel.offset.x = px_from_pt((F32)metrics.leftSideBearing / run_wrapper.design_units_per_em * run.fontEmSize);
                                cel.offset.y = px_from_pt((F32)metrics.topSideBearing / run_wrapper.design_units_per_em * run.fontEmSize);
                            }

                            hmput(*hash_table_in, glyph_index, cel);
                            arrput(glyph_cels, cel);
                        }

                        analysis->Release();
                    }
                    else  // glyph index exists in the inner-table
                    {
                        Glyph_Cel cel = hmget(*hash_table_in, glyph_index);
                        arrput(glyph_cels, cel);
                    }
                }
            }
            else // font face doesn't exist in the outer-table
            {
                Dwrite_Inner_Hash_Table **hash_table_in = new Dwrite_Inner_Hash_Table *;
                *hash_table_in = NULL;
                hmput(atlas_hash_table_out, font_face, hash_table_in);

                for (U32 i = 0; i < glyph_count; ++i)
                {
                    U16 glyph_index = run_wrapper.indices[i];
                    U128 uvs = {};

                    if (hmgeti(*hash_table_in, glyph_index) == -1) // glyph index doesn't exist in the inner-table
                    {
                        // Get single glyph's metrics.
                        DWRITE_GLYPH_METRICS metrics = {};
                        win32_assume_hr(font_face->GetDesignGlyphMetrics(&glyph_index, 1, &metrics, run.isSideways));

                        // Get blackbox of a glyph.
                        assume(run_wrapper.design_units_per_em != 0);
                        FLOAT bb_width_em = (FLOAT)(metrics.advanceWidth - metrics.leftSideBearing - metrics.rightSideBearing) / run_wrapper.design_units_per_em;
                        FLOAT bb_height_em = (FLOAT)(metrics.advanceHeight - metrics.topSideBearing - metrics.bottomSideBearing) / run_wrapper.design_units_per_em;
                        FLOAT pt_per_em = run.fontEmSize;
                        FLOAT bb_width_pt = bb_width_em * pt_per_em;
                        FLOAT bb_height_pt = bb_height_em * pt_per_em;

                        // CreateGlyphRunAnalysis() doesn't support DWRITE_RENDERING_MODE_OUTLINE.
                        // We won't bother big glyphs. (many hundreds of pt)
                        if (rendering_mode == DWRITE_RENDERING_MODE1_OUTLINE)
                        { rendering_mode = DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC; }

                        DWRITE_GLYPH_RUN single_glyph_run = {};
                        {
                            single_glyph_run.fontFace      = font_face;
                            single_glyph_run.fontEmSize    = run.fontEmSize;
                            single_glyph_run.glyphCount    = 1;
                            single_glyph_run.glyphIndices  = &glyph_index;
                            single_glyph_run.glyphAdvances = NULL;
                            single_glyph_run.glyphOffsets  = NULL;
                            single_glyph_run.isSideways    = run.isSideways;
                            single_glyph_run.bidiLevel     = run.bidiLevel;
                        }

                        IDWriteGlyphRunAnalysis *analysis = NULL;
                        win32_assume_hr(dwrite_factory->CreateGlyphRunAnalysis(&single_glyph_run,
                                                                               NULL, // transform
                                                                               rendering_mode,
                                                                               measuring_mode,
                                                                               grid_fit_mode,
                                                                               is_cleartype ? DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE : DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE,
                                                                               0.0f, // baselineOriginX
                                                                               0.0f, // baselineOriginY
                                                                               &analysis));

                        RECT bounds = {};
                        hr = analysis->GetAlphaTextureBounds(texture_type, &bounds);
                        if (FAILED(hr))
                        {
                            // @Todo: The font doesn't support DWRITE_TEXTURE_CLEARTYPE_3x1.
                            // Retry with DWRITE_TEXTURE_ALIASED_1x1.
                            assume(! "x");
                        }

                        U32 bitmap_width  = bounds.right - bounds.left;
                        U32 bitmap_height = bounds.bottom - bounds.top;
                        U32 bitmap_pitch  = is_cleartype ?  bitmap_width << 2 : bitmap_width;
                        U32 bitmap_size   = bitmap_pitch * bitmap_height;

                        if (bitmap_width == 0 || bitmap_height == 0)
                        {
                            // @Todo: Skip bitmap generation if empty.
                            assume(! "x");
                        }
                        else
                        {
                            U8 *bitmap_data_rgb = new U8[bitmap_size];
                            win32_assume_hr(analysis->CreateAlphaTexture(texture_type, &bounds, bitmap_data_rgb, bitmap_size));

                            B32 fit = false;
                            U32 x1, y1, x2, y2;
                            F32 width_px, height_px;

                            for (Binpack *partition = atlas_partition_sentinel->next;
                                 partition != atlas_partition_sentinel;
                                 partition = partition->next)
                            {
                                if (!partition->occupied)
                                {
                                    U32 w1 = partition->w;
                                    U32 h1 = partition->h;
                                    U32 w2 = bitmap_width;
                                    U32 h2 = bitmap_height;
                                    width_px = (F32)bitmap_width;
                                    height_px = (F32)bitmap_height;

                                    if (w1 >= w2 && h1 >= h2)
                                    {
                                        x1 = partition->x;
                                        y1 = partition->y;
                                        x2 = x1 + w2;
                                        y2 = y1 + h2;
                                        fit = true;

                                        U32 dx[3] = {w2, 0, w2};
                                        U32 dy[3] = {0, h2, h2};
                                        U32 nw[3] = {w1-w2, w2, w1-w2};
                                        U32 nh[3] = {h2, h1-h2, h1-h2};

                                        for (U32 npi = 0; npi < 3; ++npi)
                                        {
                                            Binpack *new_partition = new Binpack;
                                            new_partition->occupied = false;
                                            new_partition->x = x1 + dx[npi];
                                            new_partition->y = y1 + dy[npi];
                                            new_partition->w = nw[npi];
                                            new_partition->h = nh[npi];
                                            dll_append(atlas_partition_sentinel, new_partition);
                                        }

                                        partition->occupied = true;
                                        partition->w = w2;
                                        partition->h = h2;

                                        break;
                                    }
                                }
                            }

                            if (fit)
                            {
                                // RGB to RGBA
                                for (UINT32 r = 0; r < bitmap_height; ++r)
                                {
                                    for (UINT32 c = 0; c < bitmap_width; ++c)
                                    {
                                        BYTE *dst = atlas_data + (y1+r)*atlas_pitch + (x1+c)*4;
                                        BYTE *src = bitmap_data_rgb + r*bitmap_width*3 + c*3;
                                        *(UINT32 *)dst = *(UINT32 *)src;
                                        if (src[0] == 0 && src[1] == 0  && src[2] == 0)
                                        { dst[3] = 0x00; }
                                        else
                                        { dst[3] = 0xff; }
                                    }
                                }
                            }
                            else
                            {
                                assume(! "x");
                            }

                            delete [] bitmap_data_rgb;

                            Glyph_Cel cel = {};
                            {
                                cel.uv_min = {(F32)x1 / (F32)atlas_width, (F32)y1 / (F32)atlas_height};
                                cel.uv_max = {(F32)x2 / (F32)atlas_width, (F32)y2 / (F32)atlas_height};
                                cel.width_px = width_px;
                                cel.height_px = height_px;
                                cel.offset.x = px_from_pt((F32)metrics.leftSideBearing / run_wrapper.design_units_per_em * run.fontEmSize);
                                cel.offset.y = px_from_pt((F32)metrics.topSideBearing / run_wrapper.design_units_per_em * run.fontEmSize);
                            }
                            hmput(*hash_table_in, glyph_index, cel);
                            arrput(glyph_cels, cel);
                        }

                        analysis->Release();
                    }
                    else  // glyph index exists in the inner-table
                    {
                        Glyph_Cel cel = hmget(*hash_table_in, glyph_index);
                        arrput(glyph_cels, cel);
                    }
                }
            }
        }

        // -----------------------------------------
        // @Note: Render text per container.

        V2 offset_pt = {100.0f, 300.0f}; 
        V2 offset_px = px_from_pt(offset_pt);

        static F32 time = 0.0f;
        time += dt;
        container_width_pt = 100 * (sinf(time)*0.5f + 1.5f);
        container_width_px = px_from_pt(container_width_pt);

        render_quad_px_min_max(V2{offset_px.x, offset_px.y - container_height_px},
                               V2{offset_px.x + container_width_px, offset_px.y});

        V2 origin_pt = {};

        for (U32 run_idx = 0; run_idx < run_count; ++run_idx)
        {
            Dwrite_Glyph_Run run_wrapper = glyph_runs[run_idx];
            DWRITE_GLYPH_RUN run = run_wrapper.run;

            U64 glyph_count = arrlenu(run_wrapper.indices);
            for (U32 gi = 0; gi < glyph_count; ++gi)
            {
                F32 advance_pt = run_wrapper.advances[gi];

                if (gi < glyph_count - 1)
                {
                    F32 next_glyph_advance = run_wrapper.advances[gi + 1];

                    if (origin_pt.x + advance_pt + next_glyph_advance /*@Todo: - next_rsb*/ > container_width_pt)
                    {
                        origin_pt.x = 0.0f;
                        origin_pt.y -= run_wrapper.vertical_advance_pt;
                    }
                }

                Glyph_Cel cel = glyph_cels[gi];

                V2 origin_px = px_from_pt(origin_pt + offset_pt);
                V2 min_px = origin_px;
                {
                    min_px.x += cel.offset.x;
                    //min_px.y += cel.offset.y;
                }
                V2 max_px = min_px;
                {
                    max_px.x += cel.width_px;
                    max_px.y += cel.height_px;
                }

                V2 uv_min = {cel.uv_min.x, cel.uv_max.y};
                V2 uv_max = {cel.uv_max.x, cel.uv_min.y};

                render_texture(min_px, max_px, uv_min, uv_max);

                origin_pt.x += advance_pt;
            }
        }

        // -----------------------
        // @Note: D3D11 Pass

        // @Temporary(lsw)
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
