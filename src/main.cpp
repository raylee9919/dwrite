// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

#define OS_WINDOWS
#include "include/codebase.h"

//------------------------------------
// Note: [.h]
#define STB_DS_IMPLEMENTATION
#define STBDS_ASSERT
#include "third_party/stb_ds.h"

#include "sw_dwrite.h"
#include "render.h"

//------------------------------------
// Note: [.cpp]
#include "sw_dwrite.cpp"
#include "render.cpp"

//------------------------------------
// Note: Generated HLSL byte code.
#include "shaders/shader_vs.h"
#include "shaders/shader_ps.h"

#include "shaders/panel_vs.h"
#include "shaders/panel_ps.h"

#define win32_assume_hr(hr) assume(SUCCEEDED(hr))

global B32 should_accumulate_time = false;

//------------------------------------
// @Todo: 1. Validate high-dpi.
//        2. Kerning. Test Case: EB Goromond, "LoTR", Feel like 'T' must be closer to 'o'.

typedef struct Bin Bin;
struct Bin
{
    Bin *prev;
    Bin *next;
    B32 occupied;
    U32 x, y, w, h;
};

function void
dwrite_pack_glyphs_in_run_to_atlas(IDWriteFactory3 *dwrite_factory,
                                   B32 is_cleartype,
                                   DWRITE_GLYPH_RUN run,
                                   DWRITE_RENDERING_MODE1 rendering_mode,
                                   DWRITE_MEASURING_MODE measuring_mode,
                                   DWRITE_GRID_FIT_MODE grid_fit_mode,
                                   Dwrite_Inner_Hash_Table **hash_table_in,
                                   Bitmap atlas,
                                   Bin *atlas_partition_sentinel,
                                   Glyph_Cel_Array *glyph_cels)
{
    HRESULT hr = S_OK;

    Temporary_Arena scratch = scratch_begin();

    U64 glyph_count = arrlenu(run.glyphIndices);
    IDWriteFontFace5 *font_face = (IDWriteFontFace5 *)run.fontFace;
    assert(hmgeti(dwrite_font_hash_table, run.fontFace) != -1);
    Dwrite_Font_Metrics font_metrics = hmget(dwrite_font_hash_table, run.fontFace);

    //F32 em_per_du = 1.0f / font_metrics.du_per_em;
    //F32 px_per_du = run.fontEmSize * em_per_du;

    DWRITE_TEXTURE_TYPE texture_type = (is_cleartype) ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;

    // Check if each glyph in the run exists in the inner hash table.
    for (U32 i = 0; i < glyph_count; ++i)
    {
        U16 glyph_index = run.glyphIndices[i];

        if (hmgeti(*hash_table_in, glyph_index) == -1) // glyph index doesn't exist in the inner-table
        {
            // Get single glyph's metrics.
            DWRITE_GLYPH_METRICS metrics = {};
            win32_assume_hr(font_face->GetDesignGlyphMetrics(&glyph_index, 1, &metrics, run.isSideways));

            // CreateGlyphRunAnalysis() doesn't support DWRITE_RENDERING_MODE_OUTLINE.
            // We won't bother big glyphs. (many hundreds of pt)
            if (rendering_mode == DWRITE_RENDERING_MODE1_OUTLINE)
            {
                rendering_mode = DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC; 
            }

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

            // @Note: GetAlphaTextureBounds() -> RECT exaplanation.
            //
            // bounds.top ------++-----######--+
            //   (-7)           ||  ############
            //                  ||####      ####
            //                  |###       #####
            //  baseline ______ |###      #####|
            //   origin        \|############# |
            //  (= 0,0)         \|###########  |
            //                  ++-------###---+
            //                  ##      ###    |
            // bounds.bottom ---+#########-----+
            //    (+2)          |              |
            //             bounds.left     bounds.right
            //                 (-1)           (+14)
            //

            RECT bounds = {};
            hr = analysis->GetAlphaTextureBounds(texture_type, &bounds);
            if (FAILED(hr))
            {
                // @Todo: The font doesn't support DWRITE_TEXTURE_CLEARTYPE_3x1.
                // Retry with DWRITE_TEXTURE_ALIASED_1x1.
                assume(! "x");
            }

            Glyph_Cel cel = {};

            if ((bounds.right > bounds.left) && (bounds.bottom > bounds.top))
            {
                U32 blackbox_width  = bounds.right - bounds.left;
                U32 blackbox_height = bounds.bottom - bounds.top;

                U32 margin = 1;
                U32 bitmap_width  = blackbox_width + 2*margin;
                U32 bitmap_height = blackbox_height + 2*margin;

                U32 rgb_bitmap_size = (is_cleartype) ? (blackbox_width*3)*blackbox_height : blackbox_width*blackbox_height; 
                U8 *bitmap_data_rgb = (U8 *)arena_push(scratch.arena, rgb_bitmap_size);
                win32_assume_hr(analysis->CreateAlphaTexture(texture_type, &bounds, bitmap_data_rgb, rgb_bitmap_size));

                B32 fit = false;
                U32 x1 = 0;
                U32 y1 = 0;
                U32 x2 = 0;
                U32 y2 = 0;

                dll_for(atlas_partition_sentinel, partition)
                {
                    if (!partition->occupied)
                    {
                        U32 w1 = partition->w;
                        U32 h1 = partition->h;
                        U32 w2 = bitmap_width;
                        U32 h2 = bitmap_height;

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
                                Bin *new_partition = new Bin;
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
                    for (U32 r = 0; r < blackbox_height; ++r)
                    {
                        for (U32 c = 0; c < blackbox_width; ++c)
                        {
                            U8 *dst = atlas.data + (y1+r+margin)*atlas.pitch + (x1+c+margin)*4;
                            U8 *src = bitmap_data_rgb + r*blackbox_width*3 + c*3;
                            *(U32 *)dst = *(U32 *)src;
#if 0
                            if (src[0] == 0 && src[1] == 0  && src[2] == 0)
                            {
                                dst[3] = 0x00; 
                            }
                            else
                            {
                                dst[3] = 0xff; 
                            }
#else
                            dst[3] = 0xff; 
#endif
                        }
                    }
                }
                else
                {
                    assume(! "x");
                }

                cel.is_empty        = false;
                cel.uv_min       = {(F32)(x1 + margin) / (F32)atlas.width, (F32)(y1 + margin) / (F32)atlas.height};
                cel.uv_max       = {(F32)(x2 - margin) / (F32)atlas.width, (F32)(y2 - margin) / (F32)atlas.height};
                cel.width_px     = (F32)blackbox_width;
                cel.height_px    = (F32)blackbox_height;
                cel.offset_px.x  = (F32)bounds.left;
                cel.offset_px.y  = (F32)-bounds.top;
            }
            else
            {
                cel.is_empty        = true;
                cel.uv_min       = V2{0.0f, 0.0f};
                cel.uv_max       = V2{0.0f, 0.0f};
                cel.width_px     = 0.0f;
                cel.height_px    = 0.0f;
                cel.offset_px.x  = 0.0f;
                cel.offset_px.y  = 0.0f;
            }

            darr_push(glyph_cels, cel);
            hmput(*hash_table_in, glyph_index, cel);

            analysis->Release();
        }
        else  // glyph index exists in the inner-table
        {
            Glyph_Cel cel = hmget(*hash_table_in, glyph_index);
            darr_push(glyph_cels, cel);
        }
    }

    scratch_end(scratch);
}

function int
main_entry(void)
{
    Arena *permanent_arena = arena_alloc();
    F64 counter_frequency_inverse = (1.0 / (F64)os_query_timer_frequency());

    Os_Window *window = os_create_window(1920, 1080, L"fönster");
    if (! window)
    {
        // @Fix: focus to caption.
        os_gui_message(L"CAPTION", L"Could not create window");
        os_abort();
    }

    // -------------------------------
    // @Note: Init DWrite
    F32 pt_per_em   = 80.0f;
    F32 px_per_inch = (F32)os_get_dpi(window);

    IDWriteFactory3 *dwrite_factory = NULL;
    win32_assume_hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(dwrite_factory), (IUnknown **)&dwrite_factory));

    IDWriteFontCollection *font_collection = NULL;
    win32_assume_hr(dwrite_factory->GetSystemFontCollection(&font_collection));

    Dwrite_Outer_Hash_Table *atlas_hash_table_out = NULL;

    WCHAR *fonts[] = 
    {
        L"Fira Code",           // 0
        L"Consolas",            // 1
        L"Times New Roman",     // 2
        L"Roboto Mono",         // 3
        L"Zapfino",             // 4
        L"EB Garamond",         // 5
    };
    WCHAR *base_font_family_name = fonts[5];
    U32 family_index = 0;
    BOOL family_exists = FALSE;
    win32_assume_hr(font_collection->FindFamilyName(base_font_family_name, &family_index, &family_exists));

    assume(family_exists);

    // @Note: IDWriteTextLayout allows you to map a string and a base font into chunks of string with a resolved font.
    //        For example, if there's a emoji in the string, it would automatically fallback from the builtin font to Segoe UI (the emoji font on Windows). 
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
    { memory_copy(locale, L"en-US", sizeof(L"en-US")); }



    // -----------------------------
    // @Note: Prepare DWrite

    IDWriteRenderingParams *rendering_params = NULL;
    win32_assume_hr(dwrite_factory->CreateRenderingParams(&rendering_params));

    B32 is_cleartype = TRUE;
    DWRITE_GLYPH_RUN *glyph_runs = NULL;

    Bitmap atlas = {};
    {
        atlas.width  = 1024;
        atlas.height = 1024;
        atlas.pitch  = (atlas.width << 2);
        atlas.data   = (U8 *)arena_push(permanent_arena, atlas.pitch*atlas.height);
    }

    Bin *atlas_partition_sentinel = arena_push_struct(permanent_arena, Bin);
    {
        Bin *head = arena_push_struct(permanent_arena, Bin);
        {
            head->prev = atlas_partition_sentinel;
            head->next = atlas_partition_sentinel;
            head->occupied = false;
            head->x = 0;
            head->y = 0;
            head->w = atlas.width;
            head->h = atlas.height;
        }
        atlas_partition_sentinel->prev = head;
        atlas_partition_sentinel->next = head;
    }


    // -----------------------------
    // @Hack:
    d3d11_init(win32_get_hwnd(window));



    // -----------------------------
    // @Note: Create Vertex Shader
    ID3D11VertexShader *vertex_shader = NULL;
    { win32_assume_hr(d3d11.device->CreateVertexShader(g_vs_main, sizeof(g_vs_main), NULL, &vertex_shader)); }

    ID3D11VertexShader *panel_vs = NULL;
    { win32_assume_hr(d3d11.device->CreateVertexShader(g_panel_vs_main, sizeof(g_panel_vs_main), NULL, &panel_vs)); }


    // -----------------------------
    // @Note: Create Pixel Shader
    ID3D11PixelShader *pixel_shader = NULL;
    { win32_assume_hr(d3d11.device->CreatePixelShader(g_ps_main, sizeof(g_ps_main), NULL, &pixel_shader)); }

    ID3D11PixelShader *panel_ps = NULL;
    { win32_assume_hr(d3d11.device->CreatePixelShader(g_panel_ps_main, sizeof(g_panel_ps_main), NULL, &panel_ps)); }



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


    V2U client_size   = os_get_client_size(window);
    U32 window_width  = client_size.x;
    U32 window_height = client_size.y;

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
        sampler_desc.MaxAnisotropy  = 1;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sampler_desc.MaxLOD         = D3D11_FLOAT32_MAX;
    }

    ID3D11SamplerState* sampler_state;
    d3d11.device->CreateSamplerState(&sampler_desc, &sampler_state);



    // ------------------------------
    // @Note: Create Texture
    ID3D11Texture2D *d3d_atlas = NULL;
    {
        D3D11_TEXTURE2D_DESC texture_desc = {};
        {
            texture_desc.Width              = atlas.width;
            texture_desc.Height             = atlas.height;
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
            texture_subresource_data.pSysMem      = atlas.data;
            texture_subresource_data.SysMemPitch  = atlas.pitch;
        }

        d3d11.device->CreateTexture2D(&texture_desc, &texture_subresource_data, &d3d_atlas);
    }

    ID3D11ShaderResourceView *texture_view = NULL;
    d3d11.device->CreateShaderResourceView(d3d_atlas, NULL, &texture_view);


    // ------------------------------
    // @Note: Create Blend State

    ID3D11BlendState *blend_state = NULL;
    {
        D3D11_BLEND_DESC blend_desc = {};
        {
            D3D11_RENDER_TARGET_BLEND_DESC target_blend_desc = {};
            {
                target_blend_desc.BlendEnable           = TRUE;
                target_blend_desc.SrcBlend              = D3D11_BLEND_SRC_ALPHA;
                target_blend_desc.DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
                target_blend_desc.BlendOp               = D3D11_BLEND_OP_ADD;
                target_blend_desc.SrcBlendAlpha         = D3D11_BLEND_ONE;
                target_blend_desc.DestBlendAlpha        = D3D11_BLEND_ONE;
                target_blend_desc.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
                target_blend_desc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            }
            blend_desc.AlphaToCoverageEnable  = FALSE;
            blend_desc.IndependentBlendEnable = FALSE;
            blend_desc.RenderTarget[0]        = target_blend_desc;
        }
        assume(SUCCEEDED(d3d11.device->CreateBlendState(&blend_desc, &blend_state)));
    }

    ID3D11BlendState *glyph_blend_state = NULL;
    {
        D3D11_BLEND_DESC blend_desc = {};
        {
            D3D11_RENDER_TARGET_BLEND_DESC target_blend_desc = {};
            {
                target_blend_desc.BlendEnable           = TRUE;
                target_blend_desc.SrcBlend              = D3D11_BLEND_ONE;
                target_blend_desc.DestBlend             = D3D11_BLEND_ONE;
                target_blend_desc.BlendOp               = D3D11_BLEND_OP_MAX;
                target_blend_desc.SrcBlendAlpha         = D3D11_BLEND_ONE;
                target_blend_desc.DestBlendAlpha        = D3D11_BLEND_INV_SRC1_ALPHA;
                target_blend_desc.BlendOpAlpha          = D3D11_BLEND_OP_ADD;
                target_blend_desc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            }
            blend_desc.AlphaToCoverageEnable  = FALSE;
            blend_desc.IndependentBlendEnable = FALSE;
            blend_desc.RenderTarget[0]        = target_blend_desc;
        }
        assume(SUCCEEDED(d3d11.device->CreateBlendState(&blend_desc, &glyph_blend_state)));
    }

    U64 last_counter;
    {
        LARGE_INTEGER li = {};
        QueryPerformanceCounter(&li);
        last_counter = li.QuadPart;
    }

    WCHAR *text = arena_push_array(permanent_arena, WCHAR, 65536);
    U32 text_length = 0;

    //text=L"But thereupon Éomer rode up in haste... and grief and dismay fell upon him as he came to the king's side and stood there in silence....And he looked at the slain, recalling their names. Then suddenly he beheld his sister Éowyn as she lay, and he knew her. He stood a moment as a man who is pierced in the midst of a cry by an arrow through the heart; and then his face went deathly white; and a cold fury rose in him, so that all speech failed him for a while. A fey mood took him. 'Éowyn, Éowyn!' he cried at last: 'Éowyn, how come you here? What madness or devilry is this? Death, death, death! Death take us all!' Then without taking counsel or waiting for the approach of the men of the City, he spurred headlong back to the front of the great host, and blew a horn, and cried aloud for the onset. Over the field rang his clear voice calling: 'Death! Ride, ride to ruin and the world's ending!' And with that the host began to move. But the Rohirrim sang no more. Death they cried with one voice loud and terrible, and gathering speed like a great tide their battle swept about their fallen king and passed, roaring away southwards. - LoTR: The Return of the King";
    //text=L"->";
    text=L"LoTR";
    text_length = (U32)wcslen(text);

    // ------------------------------
    // @Note: Main Loop
    Arena *frame_arena = arena_alloc();

    while (! window->should_close)
    {
        for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);)
        {
            switch (msg.message)
            {
                case WM_CHAR: {
                    should_accumulate_time = !should_accumulate_time;
                } break;

                default: {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                } break;
            }
        }

        // ---------------------------
        // @Note: Query new dt
        U64 new_counter = os_read_timer();
        F64 dt = (F64)(new_counter - last_counter) * counter_frequency_inverse;
        last_counter = new_counter;

        char buf[256];
        snprintf(buf, sizeof(buf), "dt: %.6f\n", dt);
        OutputDebugString(buf);

        static F64 time = 0.0;
        if (should_accumulate_time)
        { time += dt; }

        // ---------------------------
        // @Note: Update
        arena_clear(frame_arena);

        renderer.vertex_count = 0;
        renderer.index_count  = 0;

        Glyph_Cel_Array glyph_cels = {};
        darr_init(&glyph_cels, frame_arena);

        // -----------------------------
        // @Note: Text container.
#if 1
        F32 container_width_px  = (sinf((F32)time*0.9f)*0.5f+0.5f) * (F32)window_width;
        F32 container_height_px = (cosf((F32)time*0.7f)*0.5f+0.5f) * (F32)window_height;
#else
        F32 container_width_px  = 1000.0f;
        F32 container_height_px = 300.0f;
#endif

        V2 container_origin_px  = V2{((F32)window_width - container_width_px)*0.5f, ((F32)window_height + container_height_px)*0.5f};

        if (glyph_runs)
        {
            arrfree(glyph_runs); 
        }
        glyph_runs = dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, locale, base_font_family_name, pt_per_em, px_per_inch, text, text_length);


        U64 run_count = arrlenu(glyph_runs);
        for (U32 run_idx = 0; run_idx < run_count; ++run_idx)
        {
            DWRITE_GLYPH_RUN run = glyph_runs[run_idx];
            IDWriteFontFace5 *font_face = (IDWriteFontFace5 *)run.fontFace;

            assert(hmgeti(dwrite_font_hash_table, (U64)font_face) != -1);
            Dwrite_Font_Metrics font_metrics = hmget(dwrite_font_hash_table, (U64)font_face);

            // @Note: Create rendering mode of a font face.
            DWRITE_RENDERING_MODE1 rendering_mode = DWRITE_RENDERING_MODE1_NATURAL;
            DWRITE_MEASURING_MODE measuring_mode  = DWRITE_MEASURING_MODE_NATURAL;
            DWRITE_GRID_FIT_MODE grid_fit_mode    = DWRITE_GRID_FIT_MODE_DEFAULT;

            win32_assume_hr(font_face->GetRecommendedRenderingMode(run.fontEmSize,
                                                                   px_per_inch, px_per_inch,
                                                                   NULL, // transform
                                                                   run.isSideways,
                                                                   DWRITE_OUTLINE_THRESHOLD_ANTIALIASED,
                                                                   measuring_mode,
                                                                   rendering_params,
                                                                   &rendering_mode,
                                                                   &grid_fit_mode));


            // Look into 2-tier hash table.
            if (hmgeti(atlas_hash_table_out, (U64)font_face) != -1/*exists*/)
            {
                Dwrite_Inner_Hash_Table **hash_table_in = hmget(atlas_hash_table_out, font_face);
                dwrite_pack_glyphs_in_run_to_atlas(dwrite_factory, is_cleartype, run,
                                                   rendering_mode, measuring_mode, grid_fit_mode,
                                                   hash_table_in, atlas, atlas_partition_sentinel, &glyph_cels);
            }
            else // font face doesn't exist in the outer-table
            {
                Dwrite_Inner_Hash_Table **hash_table_in = new Dwrite_Inner_Hash_Table *;
                *hash_table_in = NULL;
                hmput(atlas_hash_table_out, (U64)font_face, hash_table_in);
                dwrite_pack_glyphs_in_run_to_atlas(dwrite_factory, is_cleartype, run,
                                                   rendering_mode, measuring_mode, grid_fit_mode,
                                                   hash_table_in, atlas, atlas_partition_sentinel, &glyph_cels);
            }
        }

        // -----------------------------------------
        // @Note: Render text per container.

        F32 max_advance_height_px = 0.0f;
        for (U32 ri = 0; ri < run_count; ++ri)
        {
            DWRITE_GLYPH_RUN run = glyph_runs[ri];
            assert(hmgeti(dwrite_font_hash_table, run.fontFace) != -1);
            // @Hack:
            max_advance_height_px = max(max_advance_height_px, hmget(dwrite_font_hash_table, run.fontFace).advance_height_px);
        }

        V2 origin_local_px = {};
        V2 origin_translate_px = container_origin_px;
        origin_translate_px.y -= max_advance_height_px;

        { // @Temporary: Draw container
            V2 min_x = container_origin_px + V2{0.0f, -container_height_px};
            V2 max_x = min_x + V2{container_width_px, container_height_px}; 
            render_quad_px_min_max(min_x, max_x);
        }

        // @Temporary:
        for (U32 ri = 0; ri < run_count; ++ri)
        {
            DWRITE_GLYPH_RUN run = glyph_runs[ri];

            assert(hmgeti(dwrite_font_hash_table, run.fontFace) != -1);
            Dwrite_Font_Metrics metrics = hmget(dwrite_font_hash_table, run.fontFace);
            F32 advance_height_px = metrics.advance_height_px;

            U64 glyph_count = arrlenu(run.glyphIndices);
            for (U32 gi = 0; gi < glyph_count; ++gi)
            {
                Glyph_Cel cel = *((Glyph_Cel *)glyph_cels.data.base + gi);
                F32 advance_x_px = run.glyphAdvances[gi];

                // If not empty glyph,
                if (! cel.is_empty)
                {
                    // @Todo: line wrap by grapheme.
                    if (gi < glyph_count - 1)
                    {
                        Glyph_Cel next_cel = ((Glyph_Cel *)glyph_cels.data.base)[gi + 1];
                        F32 next_baseline = origin_local_px.x + advance_x_px;
                        F32 next_glyph_blackbox_end_px = next_baseline + run.glyphOffsets[gi + 1].advanceOffset + next_cel.offset_px.x + next_cel.width_px;
                        if (next_glyph_blackbox_end_px >= container_width_px)
                        {
                            origin_local_px.x = 0.0f;
                            origin_local_px.y -= advance_height_px;
                        }
                    }

                    // Translate to global(container) coordinates.
                    V2 origin_global_px = origin_local_px + origin_translate_px;

                    // @Todo: Understand those and decide if I should hoist them out.
                    origin_global_px.x += run.glyphOffsets[gi].advanceOffset;
                    origin_global_px.y += run.glyphOffsets[gi].ascenderOffset;

                    V2 min_px, max_px;
                    {
                        min_px = max_px = origin_global_px;
                        min_px.x += cel.offset_px.x;
                        max_px.x = min_px.x + cel.width_px;
                        max_px.y += cel.offset_px.y;
                        min_px.y = max_px.y - cel.height_px;
                    }

                    // Culling/Clipping.
                    AABB2 box_container = AABB2{container_origin_px, container_origin_px};
                    {
                        box_container.min.y -= container_height_px;
                        box_container.max.x += container_width_px;
                    }

                    AABB2 box_cel = AABB2{min_px, max_px};

                    // @Todo: We are not wrapping line by graphemes currently. Thus, it'll sometimes look like 
                    //        glyphs are disappearing by nowhere.
                    if (intersects(box_container, box_cel))
                    {
                        // @Todo: intersection() does some duplicate operations to intersects().
                        AABB2 overlap = intersection(box_container, box_cel);

                        V2 uv_min      = V2{cel.uv_min.x, cel.uv_max.y};
                        V2 uv_max      = V2{cel.uv_max.x, cel.uv_min.y};
                        V2 uv_range_x  = V2{cel.uv_min.x, cel.uv_max.x};
                        V2 uv_range_y  = V2{cel.uv_min.y, cel.uv_max.y};
                        V2 box_range_x = V2{box_cel.min.x, box_cel.max.x};
                        V2 box_range_y = V2{box_cel.min.y, box_cel.max.y};

                        uv_min.x = lerp(uv_range_x,  normalize01(box_range_x, overlap.min.x));
                        uv_max.x = lerp(uv_range_x,  normalize01(box_range_x, overlap.max.x));
                        uv_min.y = lerp(uv_range_y, -normalize01(box_range_y, overlap.min.y) + 1.0f);
                        uv_max.y = lerp(uv_range_y, -normalize01(box_range_y, overlap.max.y) + 1.0f);

                        render_texture(overlap.min, overlap.max, uv_min, uv_max); 
                    }
                }

                // Advance
                origin_local_px.x += advance_x_px;
            }
        }

        // -----------------------
        // @Note: D3D11 Pass

        // @Hack: Convert to NDC on CPU-side.
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
            memory_copy(mapped_subresource.pData, renderer.vertices, sizeof(renderer.vertices[0])*renderer.vertex_count);
            d3d11.device_ctx->Unmap(vertex_buffer, 0);
        }

        // ---------------------------
        // @Note: Update index buffer.
        {
            D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
            d3d11.device_ctx->Map(index_buffer, 0/*index # of subresource*/, D3D11_MAP_WRITE_DISCARD, 0/*flags*/, &mapped_subresource);
            memory_copy(mapped_subresource.pData, renderer.indices, sizeof(renderer.indices[0])*renderer.index_count);
            d3d11.device_ctx->Unmap(index_buffer, 0);
        }

        // ---------------------------
        // @Note: Update atlas.
        {
            D3D11_MAPPED_SUBRESOURCE mapped_subresource = {};
            d3d11.device_ctx->Map(d3d_atlas, 0/*index # of subresource*/, D3D11_MAP_WRITE_DISCARD, 0/*flags*/, &mapped_subresource);
            memory_copy(mapped_subresource.pData, atlas.data, sizeof(atlas.data[0])*atlas.pitch*atlas.height);
            d3d11.device_ctx->Unmap(d3d_atlas, 0);
        }


        FLOAT background_color[4] = {0.12f, 0.12f, 0.12f, 1.0f};
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

        { // Panel shader
            d3d11.device_ctx->VSSetShader(panel_vs, NULL, 0);
            d3d11.device_ctx->PSSetShader(panel_ps, NULL, 0);

            d3d11.device_ctx->OMSetRenderTargets(1, &d3d11.framebuffer_view, NULL/*Depth-Stencil View*/);
            d3d11.device_ctx->OMSetBlendState(blend_state, NULL, 0xffffffff);

            // @Hack:
            d3d11.device_ctx->DrawIndexed(6, 0/*StartIndexLocation*/, 0/*BaseVertexLocation*/);
        }

        { // Glyph shader
            d3d11.device_ctx->VSSetShader(vertex_shader, NULL, 0);
            d3d11.device_ctx->PSSetShader(pixel_shader, NULL, 0);

            d3d11.device_ctx->PSSetShaderResources(0, 1, &texture_view);
            d3d11.device_ctx->PSSetSamplers(0, 1, &sampler_state);

            d3d11.device_ctx->OMSetRenderTargets(1, &d3d11.framebuffer_view, NULL/*Depth-Stencil View*/);
            d3d11.device_ctx->OMSetBlendState(glyph_blend_state, NULL, 0xffffffff);

            // @Hack:
#if 1
            d3d11.device_ctx->DrawIndexed(renderer.index_count - 6, 6/*StartIndexLocation*/, 0/*BaseVertexLocation*/);
#else
            d3d11.device_ctx->DrawIndexed(6, 6, 0/*BaseVertexLocation*/);
            d3d11.device_ctx->DrawIndexed(6, 12, 0/*BaseVertexLocation*/);
#endif
        }

        d3d11.swapchain->Present(1, 0);
    }

    os_close_window(window);

    return 0;
}
