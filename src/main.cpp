// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

// @Study:
// COM, emSize
// Can I release base interface once queried advanced version of it?

// @Note: 
// "dwrite_2.h" minimum: Windows 8.1
// "dwrite_3.h" minimum: Windows 10 Build 16299

// @Todo:
// 1. When it comes to rasterization I'd first try and implement it without emoji support,
// because emojis add some complexity that may be distracting initially (to be precisely, it requires a call to TranslateColorGlyphRun)
// 
// 2. Select Hashtable keys

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <dwrite_3.h> 
#include <d2d1.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>

////////////////////////
// @Note: [.h]
#include "basic.h"
#define STB_DS_IMPLEMENTATION
#define STBDS_ASSERT
#include "stb_ds.h"
#include "dwrite.h"
#include "d3d11.h"

////////////////////////
// @Note: [.cpp]
#include "dwrite.cpp"
#include "d3d11.cpp"


#define win32_assume_hr(hr) assert(SUCCEEDED(hr))


static BOOL g_running = TRUE;

static LRESULT CALLBACK
win32_window_procedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (message)
    {
        case WM_CLOSE:
        case WM_DESTROY: {
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



    // Initialize D3D11.
    d3d11_init();
    d3d11_create_swapchain_and_framebuffer(hwnd);

    // Set DPI awareness mode and acquire dpi.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    UINT dpi = GetDpiForWindow(hwnd);
    assume(dpi != 0);

    BOOL is_cleartype = FALSE;
    IDWriteFactory2 *dwrite_factory = NULL;
    IDWriteFontCollection *font_collection = NULL;
    UINT32 font_family_count = 0;
    UINT32 family_index = 0;
    BOOL family_exists = FALSE;
    IDWriteFontFamily *font_family;
    IDWriteFont *font;
    IDWriteFontFace *font_face;
    DWRITE_TEXTURE_TYPE texture_type = is_cleartype ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;
    FLOAT font_size = 20.0f;

    D2D1_FACTORY_OPTIONS d2d_factory_options = {};
    ID2D1Factory *d2d_factory = NULL;


    const WCHAR *base_font_family_name = L"consolas";
    DWRITE_FONT_WEIGHT base_font_weight   = DWRITE_FONT_WEIGHT_REGULAR;
    DWRITE_FONT_STRETCH base_font_stretch = DWRITE_FONT_STRETCH_NORMAL;
    DWRITE_FONT_STYLE base_font_style     = DWRITE_FONT_STYLE_NORMAL;



    win32_assume_hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(dwrite_factory), (IUnknown **)&dwrite_factory));

    win32_assume_hr(dwrite_factory->GetSystemFontCollection(&font_collection));

    font_family_count = font_collection->GetFontFamilyCount();
    assume(font_family_count > 0);

    // @Study: Those two lines are tightly coupled. Is it a reason why if-else style is better than early return stuff?
    win32_assume_hr(font_collection->FindFamilyName(base_font_family_name, &family_index, &family_exists));
    assert(family_exists);

    win32_assume_hr(font_collection->GetFontFamily(family_index, &font_family));

    win32_assume_hr(font_family->GetFirstMatchingFont(base_font_weight, base_font_stretch, base_font_style, &font));
    win32_assume_hr(font->CreateFontFace(&font_face));

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
    const WCHAR *text = L"Hello, World!";
    UINT32 text_length = wcslen(text);


    // Create D2D Fatory.
    d2d_factory_options.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
    win32_assume_hr(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(d2d_factory), (void **)&d2d_factory));


    // Retrieve glyph runs.
    Dwrite_Glyph_Run *glyph_runs = dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, locale, 
                                                       base_font_family_name, font_size, text, text_length);



    // @Quote(lhecker): 
    // CreateAlphaTexture() is ideal because it's the lowest layer you can go with DWrite.
    // It gives you the highest level control.
    // For example, you can build your own upload queue or render to a GDI bitmap target.
    //
    // This is the lowest level approach that is technically the best.
    // It's what every serious DirectWrite application uses, including libraries like skia.
    // It straight up yields rasterized glyphs and allows you to do your own anti-aliasing.
    // Don't be fooled by `DWRITE_TEXTURE_ALIASED_1x1`: It yields grayscale textures (allegedly).
    // It effectively replaces the `DrawGlyphRun` call in the previous point, but if you just replace it 1:1 you might notice a reduction in performance.
    // This is because Direct2D internally uses a pool of upload heaps to efficiently send batches of glyphs up to the GPU memory.
    // Preferably, you'd do something similar.

    for (UINT i = 0; i < arrlenu(runs); ++i)
    {
        Dwrite_Glyph_Run *glyph_run = glyph_runs + i;

        IDWriteGlyphRunAnalysis *glyph_run_analysis= NULL;

        dwrite_factory->CreateGlyphRunAnalysis(glyph_run,
                                               NULL, // transform
                                               rendering_mode,
                                               DWRITE_MEASURING_MODE_NATURAL,
                                               grid_fit_mode,
                                               is_cleartype ? DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE : DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE,
                                               0.0f, // baselineOriginX
                                               0.0f, // baselineOriginY
                                               &glyph_run_analysis);

        const DWRITE_TEXTURE_TYPE type = cleartype ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;

        RECT bounds = {};
        hr = glyph_run_analysis->GetAlphaTextureBounds(type, &bounds);
        if (FAILED(hr))
        {
            // @Todo: The font doesn't support DWRITE_TEXTURE_CLEARTYPE_3x1.
            // Retry with DWRITE_TEXTURE_ALIASED_1x1.
            assume(! "x");
        }

        UINT32 width = bounds.right - bounds.left;
        UINT32 height = bounds.bottom - bounds.top;

        if (width == 0 || height == 0)
        {
            // @Todo: Skip bitmap generation since it is whitespace.
        }
        
        UINT64 bitmap_size = width * height;
        if (is_cleartype)
        {
            bitmap_size *= 3; // 24-bpp.
        }

        bitmap_data = new BYTE[bitmap_size];

        win32_assume_hr(glyph_run_analysis->CreateAlphaTexture(texture_type, &bounds, data, size));
    }
    

    
    while (g_running)
    {
        // Handle messages.
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
        FLOAT background_color[4] = { 0.1f, 0.2f, 0.6f, 1.0f };
        d3d11.device_ctx->ClearRenderTargetView(d3d11.framebuffer_view, background_color);

        // Get viewport dimension.
        RECT window_rect = {};
        GetClientRect(hwnd, &window_rect);
        D3D11_VIEWPORT viewport = {};
        {
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width    = (FLOAT)(window_rect.right - window_rect.left);
            viewport.Height   = (FLOAT)(window_rect.bottom - window_rect.top);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
        }

        d3d11.device_ctx->RSSetViewports(1, &viewport);
        d3d11.device_ctx->OMSetRenderTargets(1, &d3d11.framebuffer_view, NULL/*Depth-Stencil View*/);


        d3d11.swapchain->Present(1, 0);
    }


    return 0;
}
