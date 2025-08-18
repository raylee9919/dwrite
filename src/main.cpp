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

//--------------------
// @Note: [.h]
#include "basic.h"
#define STB_DS_IMPLEMENTATION
#define STBDS_ASSERT
#include "third_party/stb_ds.h"
#include "dwrite.h"
#include "d3d11.h"
#include "shader.h"

//--------------------
// @Note: [.cpp]
#include "dwrite.cpp"
#include "d3d11.cpp"


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


    // @Note: Init DWrite
    IDWriteFactory3 *dwrite_factory = NULL;

    FLOAT const pt_per_em = 70.0f; // equivalent to font size.
    FLOAT const inch_per_em = pt_per_em / 72.0f;
    FLOAT const dip_per_inch = 96.0f;
    FLOAT const dip_per_em = dip_per_inch * inch_per_em;

    // @Note: Since fallback will be performed, finding base family isn't really necessary.
    // But you may want to find if the base font-family exists in the system initially.
    const WCHAR *base_font_family_name = L"Fira Code";
    IDWriteFontCollection *font_collection = NULL;
    UINT32 family_index = 0;
    BOOL family_exists = FALSE;

    win32_assume_hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(dwrite_factory), (IUnknown **)&dwrite_factory));
    win32_assume_hr(dwrite_factory->GetSystemFontCollection(&font_collection));
    win32_assume_hr(font_collection->FindFamilyName(base_font_family_name, &family_index, &family_exists));
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



    // @Note:
    // Smiley-face emoji's codepoint(U+1F600) is bigger than 16-bits which is unit of UTF-16 and wchar.
    // That's why wcslen() returns 2 for smiley-face with /utf-8 flag set in MSVC.
    // If the flag isn't set, it'll return 5 which is equvalent to # of bytes.

    //const WCHAR *text = L"😀a";
    const WCHAR *text = L"! Hello->World ;";
    //const WCHAR *text = L"안녕 세계 !";
    //const WCHAR *text = L"مرحبا بالعالم"; // @Todo: Arabic: right-to-left
    UINT32 text_length = wcslen(text);


    // Retrieve glyph runs.
    Dwrite_Glyph_Run *glyph_runs = dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, locale, 
                                                             base_font_family_name, pt_per_em, text, text_length);


    // Create rendering parameters.
    IDWriteRenderingParams *rendering_params = NULL;
    win32_assume_hr(dwrite_factory->CreateRenderingParams(&rendering_params));

    BYTE *bitmap_data = NULL;
    UINT bitmap_width, bitmap_height, bitmap_pitch;

    BOOL is_cleartype = TRUE;

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

        // ------------------
        // @Note: RGB to RGBA
        for (UINT32 r = 0; r < bitmap_height; ++r)
        {
            for (UINT32 c = 0; c < bitmap_width; ++c)
            {
                BYTE *dst = bitmap_data + r*bitmap_pitch + c*4;
                BYTE *src = bitmap_data_24 + r*bitmap_width*3 + c*3;
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 0xff;
            }
        }

        delete [] bitmap_data_24;
    }


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
    
    FLOAT sw = (FLOAT)bitmap_width / (FLOAT)window_width;
    FLOAT sh = (FLOAT)bitmap_height / (FLOAT)window_height;

    // -----------------------------
    // @Note: Create Vertex Buffer
    ID3D11Buffer *vertex_buffer = NULL;
    UINT32 vertex_count, stride, offset;
    FLOAT vertex_data[] = { // x, y, u, v
        -sw, +sh,  0.f, 0.f,
        +sw, -sh,  1.f, 1.f,
        -sw, -sh,  0.f, 1.f,
        -sw, +sh,  0.f, 0.f,
        +sw, +sh,  1.f, 0.f,
        +sw, -sh,  1.f, 1.f
    };
    stride       = 4 * sizeof(FLOAT);
    vertex_count = sizeof(vertex_data) / stride;
    offset       = 0;

    D3D11_BUFFER_DESC vertex_buffer_desc = {};
    {
        vertex_buffer_desc.ByteWidth = sizeof(vertex_data);
        vertex_buffer_desc.Usage     = D3D11_USAGE_IMMUTABLE;
        vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    }

    D3D11_SUBRESOURCE_DATA vertex_subresource_data = { vertex_data };

    if (FAILED(d3d11.device->CreateBuffer(&vertex_buffer_desc, &vertex_subresource_data, &vertex_buffer)))
    { assert(0); } // Todo: Error-handling


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
    ID3D11Texture2D *texture = NULL;
    ID3D11ShaderResourceView *texture_view = NULL;

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
    d3d11.device->CreateShaderResourceView(texture, NULL, &texture_view);

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

        // Clear
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

        d3d11.device_ctx->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
        d3d11.device_ctx->IASetInputLayout(input_layout);
        d3d11.device_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        d3d11.device_ctx->VSSetShader(vertex_shader, NULL, 0);

        d3d11.device_ctx->PSSetShader(pixel_shader, NULL, 0);
        d3d11.device_ctx->PSSetShaderResources(0, 1, &texture_view);
        d3d11.device_ctx->PSSetSamplers(0, 1, &sampler_state);


        d3d11.device_ctx->OMSetRenderTargets(1, &d3d11.framebuffer_view, NULL/*Depth-Stencil View*/);
        d3d11.device_ctx->OMSetBlendState(blend_state, NULL, 0xffffffff);

        d3d11.device_ctx->Draw(vertex_count, 0);

        d3d11.swapchain->Present(1, 0);
    }


    return 0;
}
