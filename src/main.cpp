// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

// @STUDY:
// dpi, COM, emSize

// @NOTE: 
// "dwrite_2.h" minimum: Windows 8.1
// "dwrite_3.h" minimum: Windows 10 Build 16299

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <dwrite_3.h> 
#include <d2d1.h>

#include <stdio.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "d2d1")

#define assert(exp) do { if (!(exp)) __debugbreak(); } while(0)
#define assume assert
#define array_count(arr) (sizeof(arr) / sizeof(arr[0]))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define win32_assume_hr(hr) assert(SUCCEEDED(hr))

#include "dwrite.h"

#include "dwrite.cpp"


static void
memory_copy(void *src, void *dst, UINT64 size)
{
    memcpy(dst, src, size);
}

static LRESULT CALLBACK
win32_callback(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
        wcex.lpfnWndProc        = win32_callback;
        wcex.cbClsExtra         = 0;
        wcex.cbWndExtra         = 0;
        wcex.hInstance          = hinst;
        wcex.hIcon              = nullptr;
        wcex.hCursor            = LoadCursor(hinst, IDC_ARROW);
        wcex.hbrBackground      = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wcex.lpszMenuName       = nullptr;
        wcex.lpszClassName      = L"ClassUnnamed";
    }
    assume(RegisterClassW(&wcex));

    HWND hwnd = CreateWindowExW(0/*style->DWORD*/, wcex.lpszClassName, L"DirectWrite",
                                WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                nullptr, nullptr, hinst, nullptr);
    assume(hwnd);

    // @NOTE: Set DPI awareness mode and acquire dpi.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    UINT dpi = GetDpiForWindow(hwnd);
    assume(dpi != 0);

    BOOL is_cleartype = FALSE;
    IDWriteFactory2 *dwrite_factory = nullptr;
    IDWriteFontCollection *font_collection = nullptr;
    UINT32 font_family_count = 0;
    UINT32 family_index = 0;
    BOOL family_exists = FALSE;
    IDWriteFontFamily *font_family;
    IDWriteFont *font;
    IDWriteFontFace *font_face;
    IDWriteGlyphRunAnalysis *glyph_run_analysis;
    DWRITE_TEXTURE_TYPE texture_type = is_cleartype ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;

    D2D1_FACTORY_OPTIONS d2d_factory_options = {};
    ID2D1Factory *d2d_factory = nullptr;


    LPWSTR base_font_family_name = L"consolas";
    DWRITE_FONT_WEIGHT base_font_weight   = DWRITE_FONT_WEIGHT_REGULAR;
    DWRITE_FONT_STRETCH base_font_stretch = DWRITE_FONT_STRETCH_NORMAL;
    DWRITE_FONT_STYLE base_font_style     = DWRITE_FONT_STYLE_NORMAL;



    win32_assume_hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(dwrite_factory), (IUnknown **)&dwrite_factory));

    win32_assume_hr(dwrite_factory->GetSystemFontCollection(&font_collection));

    font_family_count = font_collection->GetFontFamilyCount();
    assume(font_family_count > 0);

    // @STUDY: Those two lines are tightly coupled. Is it a reason why if-else style is better than early return stuff?
    win32_assume_hr(font_collection->FindFamilyName(base_font_family_name, &family_index, &family_exists));
    assert(family_exists);

    win32_assume_hr(font_collection->GetFontFamily(family_index, &font_family));

    win32_assume_hr(font_family->GetFirstMatchingFont(base_font_weight, base_font_stretch, base_font_style, &font));
    win32_assume_hr(font->CreateFontFace(&font_face));

    // @NOTE: 
    // Use 'IDWriteFontFallback::MapCharacters' and 'IDWriteTextAnalyzer::GetGlyphs'.
    // Those are for 'complex glyph shaping' and are necessary for all cases where there isn't a direct 1:1 mapping between characters and glyphs. (e.g., Arabics, Ligatures)
    // 
    // 'IDWriteTextLayout' allows you to map a string and a base font into chunks of string with a resolved font.
    // For example, if there's a emoji in the string, it would automatically fallback from the builtin font to Segoe UI (the emoji font on Windows). 
    IDWriteFontFallback *font_fallback = nullptr;
    win32_assume_hr(dwrite_factory->GetSystemFontFallback(&font_fallback));

    IDWriteFontFallback1 *font_fallback1 = nullptr;
    win32_assume_hr(font_fallback->QueryInterface(__uuidof(font_fallback1), (void **)&font_fallback1));

    IDWriteTextAnalyzer *text_analyzer = nullptr;
    win32_assume_hr(dwrite_factory->CreateTextAnalyzer(&text_analyzer));

    IDWriteTextAnalyzer1 *text_analyzer1 = nullptr;
    win32_assume_hr(text_analyzer->QueryInterface(__uuidof(text_analyzer1), (void **)&text_analyzer1));

    // @NOTE: LOCALE_NAME_MAX_LENGTH includes a terminating null character.
    WCHAR locale[LOCALE_NAME_MAX_LENGTH];
    if (! GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH))
    { memory_copy(locale, L"en-US", sizeof(L"en-US")); }



    const WCHAR *text = L"😀a";
    UINT32 text_length = wcslen(text);

    TextAnalysisSource analysis_source{locale, text, text_length};


    font_fallback1->MapCharacters(&analysis_source)
                                  UINT32                    textPosition,
                                  UINT32                    textLength,
                                  font_collection, base_font_family_name, base_font_weight, base_font_style, base_font_stretch
                                  [out]          UINT32                    *mappedLength,
                                  [out]          IDWriteFont               **mappedFont,
                                  [out]          FLOAT                     *scale
                                 );

    text_analyzer->HRESULT GetGlyphs(
                                     [in]           WCHAR const                       *textString,
                                     UINT32                            textLength,
                                     IDWriteFontFace                   *fontFace,
                                     BOOL                              isSideways,
                                     BOOL                              isRightToLeft,
                                     [in]           DWRITE_SCRIPT_ANALYSIS const      *scriptAnalysis,
                                     [in, optional] WCHAR const                       *localeName,
                                     [optional]     IDWriteNumberSubstitution         *numberSubstitution,
                                     [in, optional] DWRITE_TYPOGRAPHIC_FEATURES const **features,
                                     [in, optional] UINT32 const                      *featureRangeLengths,
                                     UINT32                            featureRanges,
                                     UINT32                            maxGlyphCount,
                                     [out]          UINT16                            *clusterMap,
                                     [out]          DWRITE_SHAPING_TEXT_PROPERTIES    *textProps,
                                     [out]          UINT16                            *glyphIndices,
                                     [out]          DWRITE_SHAPING_GLYPH_PROPERTIES   *glyphProps,
                                     [out]          UINT32                            *actualGlyphCount
                                    );



    // @NOTE: Create D2D Fatory.
    d2d_factory_options.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
    win32_assume_hr(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(d2d_factory), (void **)&d2d_factory));



    // @NOTE: Create rendering params.
    IDWriteRenderingParams *base_rendering_params = nullptr;
    IDWriteRenderingParams *rendering_params = nullptr;

    win32_assume_hr(dwrite_factory->CreateRenderingParams(&base_rendering_params));

    FLOAT gamma = base_rendering_params->GetGamma();
    FLOAT enhanced_constrast = base_rendering_params->GetEnhancedContrast();
    FLOAT cleartype_level = base_rendering_params->GetClearTypeLevel();

    win32_assume_hr(dwrite_factory->CreateCustomRenderingParams(gamma, enhanced_constrast, cleartype_level, 
                                                                base_rendering_params->GetPixelGeometry(),
                                                                base_rendering_params->GetRenderingMode(),
                                                                &rendering_params));



    // @QUOTE: 
    // "This is the lowest level approach that is technically the best.
    // It's what every serious DirectWrite application uses, including libraries like skia.
    // It straight up yields rasterized glyphs and allows you to do your own anti-aliasing.
    // Don't be fooled by `DWRITE_TEXTURE_ALIASED_1x1`: It yields grayscale textures (allegedly).
    // It effectively replaces the `DrawGlyphRun` call in the previous point, but if you just replace it 1:1 you might notice a reduction in performance.
    // This is because Direct2D internally uses a pool of upload heaps to efficiently send batches of glyphs up to the GPU memory.
    // Preferably, you'd do something similar." -Leonard Hecker (Microsoft)
    //win32_assume_hr(glyph_run_analysis->CreateAlphaTexture(texture_type, &bounds, data, size));
    


    return 0;
}
