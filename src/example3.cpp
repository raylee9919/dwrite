// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <dwrite_3.h>
#include <wincodec.h>

#include <stdio.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "ole32")

#define assert(exp) do { if (!(exp)) __debugbreak(); } while(0)
#define assume assert
#define array_count(arr) (sizeof(arr) / sizeof(arr[0]))

static void
win32_abort_if_failed(HRESULT hr)
{
    if (FAILED(hr))
    {
        assert(!"Placeholder");
        //exit(1);
    }
}

int wmain(int argc, wchar_t **argv)
{
    HRESULT hr = S_OK;

    IDWriteFactory3 *dwrite_factory = nullptr;
    IDWriteFontCollection *font_collection = nullptr;

    BOOL is_cleartype = FALSE;
    wchar_t *font_name = L"Segoe UI";
    FLOAT font_size = 128;
    FLOAT dpi = 96;

    IDWriteFontFamily *font_family = nullptr;
    UINT font_family_index = 0;
    BOOL font_family_exists = FALSE;
    IDWriteFont *font = nullptr;
    IDWriteFontFace *font_face = nullptr;
    IDWriteFontFace3 *font_face2 = nullptr;

    const UINT32 codepoints[] = 
    {
        0xC6B0
    };
    const UINT len = array_count(codepoints);
    UINT16 glyph_indices[len] = {};
    FLOAT glyph_advances[len] = {};
    DWRITE_GLYPH_OFFSET glyph_offsets[len] = {};
    DWRITE_GLYPH_RUN glyph_run = {};

    IDWriteRenderingParams *rendering_params = nullptr;
    DWRITE_RENDERING_MODE1 rendering_mode = DWRITE_RENDERING_MODE1_DEFAULT;
    DWRITE_GRID_FIT_MODE grid_fit_mode = DWRITE_GRID_FIT_MODE_DEFAULT;

    RECT bounds = {};
    BYTE *bitmap_data = nullptr;
    size_t bitmap_size = 0;

    IDWriteGlyphRunAnalysis *analysis = nullptr;

    IWICImagingFactory *wic_factory = nullptr;
    IWICBitmap *wic_bitmap = nullptr;
    IWICStream *wic_stream = nullptr;
    IWICBitmapEncoder *wic_encoder = nullptr;
    IWICBitmapFrameEncode *wic_frame = nullptr;




    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(dwrite_factory), (IUnknown **)&dwrite_factory);
    win32_abort_if_failed(hr);

    hr = dwrite_factory->GetSystemFontCollection(&font_collection, FALSE);
    win32_abort_if_failed(hr);

    hr = font_collection->FindFamilyName(font_name, &font_family_index, &font_family_exists);
    win32_abort_if_failed(hr);
    assume(font_family_exists);

    hr = font_collection->GetFontFamily(font_family_index, &font_family);
    win32_abort_if_failed(hr);

    hr = font_family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
    win32_abort_if_failed(hr);

    hr = font->CreateFontFace(&font_face);
    win32_abort_if_failed(hr);

    hr = font_face->QueryInterface(__uuidof(font_face2), (void **)&font_face2);
    win32_abort_if_failed(hr);

    hr = font_face->GetGlyphIndicesW(codepoints, len, glyph_indices);
    win32_abort_if_failed(hr);


    glyph_run.fontFace       = font_face;
    glyph_run.fontEmSize     = font_size * dpi / 72.0f; // Convert font size from points (72-dpi) to pixels.
    glyph_run.glyphCount     = len;
    glyph_run.glyphIndices   = glyph_indices;
    glyph_run.glyphAdvances  = glyph_advances;
    glyph_run.glyphOffsets   = glyph_offsets;

    hr = font_face2->GetRecommendedRenderingMode(glyph_run.fontEmSize,
                                                 96.0f, 96.0f, nullptr,
                                                 glyph_run.isSideways,
                                                 DWRITE_OUTLINE_THRESHOLD_ANTIALIASED,
                                                 DWRITE_MEASURING_MODE_NATURAL,
                                                 rendering_params,
                                                 &rendering_mode,
                                                 &grid_fit_mode);

    // @NOTE: CreateGlyphRunAnalysis() doesn't support DWRITE_RENDERING_MODE_OUTLINE.
    if (rendering_mode == DWRITE_RENDERING_MODE1_OUTLINE)
    {
        // @NOTE: Don't care about large font sizes.
        rendering_mode = DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC;
    }

    dwrite_factory->CreateGlyphRunAnalysis(&glyph_run, nullptr, rendering_mode,
                                           DWRITE_MEASURING_MODE_NATURAL, grid_fit_mode,
                                           is_cleartype ? DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE : DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE,
                                           0.0f, 0.0f, &analysis);

    DWRITE_TEXTURE_TYPE dwrite_texture_type = is_cleartype ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;
    hr = analysis->GetAlphaTextureBounds(dwrite_texture_type, &bounds);
    win32_abort_if_failed(hr);

    // @TODO: If the type is DWRITE_TEXTURE_CLEARTYPE_3x1 and the call failed,
    // it indicates that the font doesn't support the type.
    // In that case, you should retry with DWRITE_TEXTURE_ALIASED_1x1.

    // @TODO: You should skip bitmap generation if the returned bounds are empty.

    UINT width  = bounds.right - bounds.left;
    UINT height = bounds.bottom - bounds.top;
    bitmap_size = width * height;
    if (is_cleartype)
    { bitmap_size *= 3; } // 24-bpp

    bitmap_data = new BYTE[bitmap_size];
    hr = analysis->CreateAlphaTexture(dwrite_texture_type, &bounds, bitmap_data, bitmap_size);
    win32_abort_if_failed(hr);

    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    win32_abort_if_failed(hr);

    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_factory));
    win32_abort_if_failed(hr);

    REFWICPixelFormatGUID wc_pixel_format = is_cleartype ? GUID_WICPixelFormat24bppRGB : GUID_WICPixelFormat8bppGray;
    UINT stride = width;
    if (is_cleartype)
    { stride *= 3; } // 24-bpp

    hr = wic_factory->CreateBitmapFromMemory(width, height, wc_pixel_format,
                                             stride, (UINT)bitmap_size, bitmap_data, &wic_bitmap);
    win32_abort_if_failed(hr);

    hr = wic_factory->CreateStream(&wic_stream);
    win32_abort_if_failed(hr);

    hr = wic_stream->InitializeFromFilename(L"output.png", GENERIC_WRITE);
    win32_abort_if_failed(hr);
    
    hr = wic_factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &wic_encoder);
    win32_abort_if_failed(hr);

    hr = wic_encoder->Initialize(wic_stream, WICBitmapEncoderNoCache);
    win32_abort_if_failed(hr);

    hr = wic_encoder->CreateNewFrame(&wic_frame, nullptr);
    win32_abort_if_failed(hr);

    hr = wic_frame->Initialize(nullptr);
    win32_abort_if_failed(hr);

    hr = wic_frame->WriteSource(wic_bitmap, nullptr);
    win32_abort_if_failed(hr);

    hr = wic_frame->Commit();
    win32_abort_if_failed(hr);

    hr = wic_encoder->Commit();
    win32_abort_if_failed(hr);

    printf("*** SUCCESSFUL! ***");
    return 0;
}
