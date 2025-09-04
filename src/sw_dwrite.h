#ifndef LSW_DWRITE_H
#define LSW_DWRITE_H

//----------------------------------------------
// @Note: 
// "dwrite_2.h" minimum: Windows 8.1
// "dwrite_3.h" minimum: Windows 10 Build 16299

#include <dwrite_3.h>

#pragma comment(lib, "dwrite.lib")

typedef struct Dwrite_Text_Analysis_Source Dwrite_Text_Analysis_Source;
struct Dwrite_Text_Analysis_Source final : IDWriteTextAnalysisSource
{
    Dwrite_Text_Analysis_Source(const wchar_t* locale, const wchar_t* text, const UINT32 textLength) noexcept : _locale{locale}, _text{text}, _text_length{textLength}
    { }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    { return 1; }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    { return 1; }

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) noexcept override
    {
        if (IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSource))) 
        {
            *ppvObject = this;
            return S_OK;
        }

        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) noexcept override
    {
        textPosition = min(textPosition, _text_length);
        *textString = _text + textPosition;
        *textLength = _text_length - textPosition;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) noexcept override
    {
        textPosition = min(textPosition, _text_length);
        *textString = _text;
        *textLength = textPosition;
        return S_OK;
    }

    DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() noexcept override
    {
        return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    }

    HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 textPosition, UINT32* textLength, const WCHAR** localeName) noexcept override
    {
        *textLength = _text_length - textPosition;
        *localeName = _locale;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution** numberSubstitution) noexcept override
    {
        return E_NOTIMPL;
    }

    const wchar_t* _locale;
    const wchar_t* _text;
    const UINT32 _text_length;
};


typedef struct Dwrite_Text_Analysis_Sink_Result Dwrite_Text_Analysis_Sink_Result; 
struct Dwrite_Text_Analysis_Sink_Result 
{
    UINT32 text_position;
    UINT32 text_length;
    DWRITE_SCRIPT_ANALYSIS analysis;
};

// DirectWrite uses an IDWriteTextAnalysisSink to inform the caller of its segmentation results. The most important part are the
// DWRITE_SCRIPT_ANALYSIS results which inform the remaining steps during glyph shaping what script ("language") is used in a piece of text.
typedef struct Dwrite_Text_Analysis_Sink Dwrite_Text_Analysis_Sink;
struct Dwrite_Text_Analysis_Sink final : IDWriteTextAnalysisSink 
{
    Dwrite_Text_Analysis_Sink_Result *results;

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    { return 1; } 

    ULONG STDMETHODCALLTYPE Release() noexcept override
    { return 1; }

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) noexcept override
    {
        if (IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSink))) 
        {
            *ppvObject = this;
            return S_OK;
        }

        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE SetScriptAnalysis(UINT32 textPosition, UINT32 textLength, const DWRITE_SCRIPT_ANALYSIS* scriptAnalysis) noexcept override
    {
        Dwrite_Text_Analysis_Sink_Result result = {};
        {
            result.text_position = textPosition;
            result.text_length   = textLength;
            result.analysis      = *scriptAnalysis;
        }

        arrput(results, result);

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetLineBreakpoints(UINT32 textPosition, UINT32 textLength, const DWRITE_LINE_BREAKPOINT* lineBreakpoints) noexcept override
    { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE SetBidiLevel(UINT32 textPosition, UINT32 textLength, UINT8 explicitLevel, UINT8 resolvedLevel) noexcept override
    { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE SetNumberSubstitution(UINT32 textPosition, UINT32 textLength, IDWriteNumberSubstitution* numberSubstitution) noexcept override
    { return E_NOTIMPL; }
};


typedef struct Dwrite_Map_Characters_Result Dwrite_Map_Characters_Result;
struct Dwrite_Map_Characters_Result 
{
    IDWriteFontFace5 *mapped_font_face;
    UINT32 mapped_length = 0;
};

typedef struct Dwrite_Map_Complexity_Result Dwrite_Map_Complexity_Result;
struct Dwrite_Map_Complexity_Result
{
    UINT16 *glyph_indices;
    UINT32 mapped_length = 0;
    BOOL is_simple = FALSE;
};

typedef struct Glyph_Cel Glyph_Cel;
struct Glyph_Cel
{
    V2  uv_min;
    V2  uv_max;
    F32 width_px;
    F32 height_px;
    V2  offset_px; // offset of a pen from baseline origin of a glyph in px.
};
typedef Dynamic_Array(Glyph_Cel) Glyph_Cel_Array;

typedef struct Dwrite_Inner_Hash_Table Dwrite_Inner_Hash_Table;
struct Dwrite_Inner_Hash_Table
{
    UINT16      key;
    Glyph_Cel   value;
};

typedef struct Dwrite_Outer_Hash_Table Dwrite_Outer_Hash_Table;
struct Dwrite_Outer_Hash_Table
{
    U64 key;
    Dwrite_Inner_Hash_Table **value;
};

typedef struct Dwrite_Font_Metrics Dwrite_Font_Metrics;
struct Dwrite_Font_Metrics
{
    F32 du_per_em;
    F32 advance_height_px;
};

typedef struct Dwrite_Font_Hash_Table Dwrite_Font_Hash_Table;
struct Dwrite_Font_Hash_Table
{
    U64 key; // (U64)(IDwriteFontFace *)
    Dwrite_Font_Metrics value;
};

typedef struct Dwrite_Font_Fallback_Result Dwrite_Font_Fallback_Result;
struct Dwrite_Font_Fallback_Result
{
    U32 length;
    IDWriteFontFace5 *font_face;
};
static Dwrite_Font_Fallback_Result
dwrite_font_fallback(IDWriteFontFallback *font_fallback,
                     IDWriteFontCollection *font_collection,
                     WCHAR *base_family, WCHAR *locale,
                     WCHAR *text, UINT32 text_length);


// -------------------
// @Note: Global Data.
static Dwrite_Font_Hash_Table *dwrite_font_hash_table;

#endif // LSW_DWRITE_H
