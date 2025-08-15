#ifndef LSW_DWRITE_H
#define LSW_DWRITE_H

// @NOTE: Thank you Leonard Hecker!
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

        *ppvObject = nullptr;
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

private:
    const wchar_t* _locale;
    const wchar_t* _text;
    const UINT32 _text_length;
};


typedef struct Dwrite_Map_Characters_Result Dwrite_Map_Characters_Result;
struct Dwrite_Map_Characters_Result 
{
    IDWriteFontFace5 *mapped_font_face;
    UINT32 mapped_length = 0;
};

typedef struct Text_Complexity_Result Text_Complexity_Result;
struct Text_Complexity_Result
{
    UINT16 *glyph_indices;
    UINT32 index_count;
    UINT32 mapped_length = 0;
    BOOL is_simple = FALSE;
};

typedef struct Owned_Glyph_Run Owned_Glyph_Run;
struct Owned_Glyph_Run 
{
    DWRITE_GLYPH_RUN glyph_run;

    IDWriteFontFace5 *font_face;

    UINT16 *indices;
    UINT32 index_count;

    FLOAT *advances;
    UINT32 advance_count;

    DWRITE_GLYPH_OFFSET *offsets;
    UINT32 offset_count;
};




#endif // LSW_DWRITE_H
