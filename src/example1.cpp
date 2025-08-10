// Copyright (c) 2025 Seong Woo Lee. All rights reserved.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwrite_3.h>

#pragma comment(lib, "dwrite.lib")

int wmain(int Argc, const wchar_t *Argv[])
{
    const bool Cleartype    = false;
    const wchar_t *FontName = L"Segoe UI";
    const float FontSize    = 128;
    const float Dpi         = 96;
    const UINT32 Codepoint  = U'y';

    IDWriteFactory *DwriteFactory = 0;
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(DwriteFactory), (IUnknown **)&DwriteFactory);

    IDWriteFontCollection *FontCollection = 0;
    DwriteFactory->GetSystemFontCollection(&FontCollection, FALSE);

    UINT32 FamilyIndex = 0;
    BOOL FamilyExists = FALSE;
    FontCollection->FindFamilyName(FontName, &FamilyIndex, &FamilyExists);
    if (! FamilyExists)
    { return 1; }

    IDWriteFontFamily *FontFamily = 0;
    FontCollection->GetFontFamily(FamilyIndex, &FontFamily);

    IDWriteFont *Font = 0;
    FontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,
                                     DWRITE_FONT_STRETCH_NORMAL,
                                     DWRITE_FONT_STYLE_NORMAL, &Font);

    IDWriteFontFace *FontFace = 0;
    Font->CreateFontFace(&FontFace);

    IDWriteFontFace3 *FontFace2 = 0;
    FontFace->QueryInterface(__uuidof(FontFace2), (void **)&FontFace2);

    UINT16 GlyphIndices[] = {0};
    UINT32 Codepoints[] = {Codepoint};
    FontFace->GetGlyphIndicesW(&Codepoints[0], 1, &GlyphIndices[0]);

    FLOAT GlyphAdvances[] = {0};
    DWRITE_GLYPH_OFFSET GlyphOffsets[] = {0};
    DWRITE_GLYPH_RUN Run = {};
    {
        Run.fontFace      = FontFace;
        Run.fontEmSize    = FontSize;
        Run.glyphCount    = 1;
        Run.glyphIndices  = GlyphIndices;
        Run.glyphAdvances = GlyphAdvances;
        Run.glyphOffsets  = GlyphOffsets;
    };

    IDWriteRenderingParams *RenderingParams = 0;
    DwriteFactory->CreateRenderingParams(&RenderingParams);

    DWRITE_RENDERING_MODE1 RenderingMode = DWRITE_RENDERING_MODE1_DEFAULT;
    DWRITE_GRID_FIT_MODE GridFitMode

    return 0;
}
