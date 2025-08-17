#pragma once

struct TextAnalysisSource final : IDWriteTextAnalysisSource {

    TextAnalysisSource(const wchar_t* locale, const wchar_t* text, const UINT32 textLength) noexcept : _locale{locale}, _text{text}, _text_length{textLength}
    {
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) noexcept override
    {
        if (IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSource))) {
            *ppvObject = this;
            return S_OK;
        }

        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) noexcept override
    {
        textPosition = std::min(textPosition, _text_length);
        *textString = _text + textPosition;
        *textLength = _text_length - textPosition;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) noexcept override
    {
        textPosition = std::min(textPosition, _text_length);
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

struct TextAnalysisSinkResult {
    uint32_t text_position;
    uint32_t text_length;
    DWRITE_SCRIPT_ANALYSIS analysis;
};

// DirectWrite uses an IDWriteTextAnalysisSink to inform the caller of its segmentation results. The most important part are the
// DWRITE_SCRIPT_ANALYSIS results which inform the remaining steps during glyph shaping what script ("language") is used in a piece of text.
struct TextAnalysisSink final : IDWriteTextAnalysisSink {
    std::vector<TextAnalysisSinkResult> results;

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) noexcept override
    {
        if (IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSink))) {
            *ppvObject = this;
            return S_OK;
        }

        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE SetScriptAnalysis(UINT32 textPosition, UINT32 textLength, const DWRITE_SCRIPT_ANALYSIS* scriptAnalysis) noexcept override
    {
        results.emplace_back(textPosition, textLength, *scriptAnalysis);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetLineBreakpoints(UINT32 textPosition, UINT32 textLength, const DWRITE_LINE_BREAKPOINT* lineBreakpoints) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE SetBidiLevel(UINT32 textPosition, UINT32 textLength, UINT8 explicitLevel, UINT8 resolvedLevel) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE SetNumberSubstitution(UINT32 textPosition, UINT32 textLength, IDWriteNumberSubstitution* numberSubstitution) noexcept override
    {
        return E_NOTIMPL;
    }
};

struct MapCharactersResult {
    com_ptr<IDWriteFontFace5> mapped_font_face;
    u32 mapped_length = 0;
};

// This function performs an iterative "font fallback" starting at text_offset.
// In most cases it'll simply look up the given base_family in the given font_collection and return that (= your primary font).
// But if the base_family doesn't have glyphs for the given text, it'll find another, alternative font and return that one.
// For instance, if the text contains emojis but your font doesn't have any, it'll return a font that does (= e.g. Segoe UI Emoji).
static MapCharactersResult
dwrite_map_characters(IDWriteFontFallback1* font_fallback,
                      IDWriteFontCollection* font_collection,
                      const wchar_t* locale, const wchar_t* base_family, const wchar_t* text, u32 text_length, u32 text_offset)
{
    TextAnalysisSource analysis_source{locale, text, text_length};
    MapCharactersResult result;

    // I'm not aware of any font that affects the scale.
    // I believe it's safe to ignore it in practice.
    f32 scale;

    // DirectWrite implements 2 font fallback models:
    // * IDWriteFontFallback implements the weight-stretch-style model.
    // * IDWriteFontFallback1 implements the more modern typographic model (Windows 10 RS3 and later).
    THROW_IF_FAILED(font_fallback->MapCharacters(
        /* analysisSource     */ &analysis_source,
        /* textPosition       */ text_offset,
        /* textLength         */ text_length,
        /* baseFontCollection */ font_collection,
        /* baseFamilyName     */ base_family,
        /* fontAxisValues     */ nullptr,
        /* fontAxisValueCount */ 0,
        /* mappedLength       */ &result.mapped_length,
        /* scale              */ &scale,
        /* mappedFontFace     */ result.mapped_font_face.addressof()
    ));

    return result;
}

struct TextComplexityResult {
    std::vector<u16> glyph_indices;
    u32 mapped_length = 0;
    BOOL is_simple = FALSE;
};

// Glyph shaping can be extremely expensive. To reduce the cost, DirectWrite has GetTextComplexity().
// Given a piece of text it'll determine the longest run of characters that map 1:1 to glyphs without any ambiguity.
// In that case it'll return is_simple = TRUE and you can use the returned glyph_indices immediately (see dwrite_map_glyphs_simple).
// Otherwise, you must perform full glyph shaping using dwrite_map_glyphs_complex.
static TextComplexityResult
dwrite_map_text_complexity(IDWriteTextAnalyzer1* text_analyzer,
                           IDWriteFontFace* font_face,
                           const wchar_t* text, u32 text_length)
{
    TextComplexityResult result;
    result.glyph_indices.resize(text_length);

    THROW_IF_FAILED(text_analyzer->GetTextComplexity(
        /* textString     */ text,
        /* textLength     */ text_length,
        /* fontFace       */ font_face,
        /* isTextSimple   */ &result.is_simple,
        /* textLengthRead */ &result.mapped_length,
        /* glyphIndices   */ result.glyph_indices.data()
    ));

    return result;
}

struct OwnedGlyphRun {
    DWRITE_GLYPH_RUN glyph_run;

    com_ptr<IDWriteFontFace5> font_face;
    std::vector<u16> glyph_indices;
    std::vector<f32> glyph_advances;
    std::vector<DWRITE_GLYPH_OFFSET> glyph_offsets;
};

// This is the 2nd half of dwrite_map_text_complexity. It computes the glyph advances.
static void
dwrite_map_glyphs_simple(IDWriteFontFace5* font_face, f32 font_size, const u16* indices, u32 indices_count, OwnedGlyphRun& run)
{
    DWRITE_FONT_METRICS1 font_metrics;
    font_face->GetMetrics(&font_metrics);

    std::vector<INT32> design_advances;
    design_advances.resize(indices_count);
    THROW_IF_FAILED(font_face->GetDesignGlyphAdvances(indices_count, indices, design_advances.data(), FALSE));

    auto total_glyph_count = run.glyph_indices.size();
    run.glyph_indices.insert(run.glyph_indices.end(), indices, indices + indices_count);
    run.glyph_advances.resize(total_glyph_count + indices_count);
    run.glyph_offsets.resize(total_glyph_count + indices_count);

    const auto scale = font_size / font_metrics.designUnitsPerEm;
    for (size_t i = 0; i < indices_count; i++) {
        run.glyph_advances[total_glyph_count++] = design_advances[i] * scale;
    }
}

// This function performs proper (complex) glyph shaping. Since this function performs only the
// most simple form of glyph shaping (i.e. not justification, etc.), it requires just 3 steps.
static void
dwrite_map_glyphs_complex(IDWriteTextAnalyzer1* text_analyzer, const wchar_t* locale,
                          IDWriteFontFace5* font_face, f32 font_size, const wchar_t* text, u32 text_length, OwnedGlyphRun& run)
{
    TextAnalysisSource analysis_source{locale, text, text_length};
    TextAnalysisSink analysis_sink;
    std::vector<u16> cluster_map;
    std::vector<DWRITE_SHAPING_TEXT_PROPERTIES> text_props;
    std::vector<DWRITE_SHAPING_GLYPH_PROPERTIES> glyph_props;

    // This equation comes from the GetGlyphs() documentation.
    size_t total_glyph_count = run.glyph_indices.size();
    const auto estimated_final_glyph_count = total_glyph_count + (3 * text_length) / 2 + 16;
    run.glyph_indices.resize(estimated_final_glyph_count);
    run.glyph_advances.resize(estimated_final_glyph_count);
    run.glyph_offsets.resize(estimated_final_glyph_count);

    // Step 1: Split the text into runs of the same script ("language"), BiDi, etc.
    // This only performs the absolute minimum with AnalyzeScript. Other Analyze* methods are available.
    THROW_IF_FAILED(text_analyzer->AnalyzeScript(&analysis_source, 0, text_length, &analysis_sink));

    for (const auto& analysis_result : analysis_sink.results) {
        // This equation comes from the GetGlyphs() documentation.
        auto estimated_glyph_count = (3 * analysis_result.text_length) / 2 + 16;
        auto estimated_glyph_count_next = total_glyph_count + estimated_glyph_count;

        // Fulfill the "_Out_writes_(...)" requirements of GetGlyphs().
        if (cluster_map.size() < analysis_result.text_length) {
            cluster_map.resize(analysis_result.text_length);
            text_props.resize(analysis_result.text_length);
        }
        if (run.glyph_indices.size() < estimated_glyph_count_next) {
            run.glyph_indices.resize(estimated_glyph_count_next);
        }
        if (glyph_props.size() < estimated_glyph_count) {
            glyph_props.resize(estimated_glyph_count);
        }

        u32 actual_glyph_count = 0;

        // Step 2: Map the given text into glyph indices.
        for (int retry = 0;;) {
            const auto hr = text_analyzer->GetGlyphs(
                /* textString          */ text + analysis_result.text_position,
                /* textLength          */ analysis_result.text_length,
                /* fontFace            */ font_face,
                /* isSideways          */ false,
                /* isRightToLeft       */ 0,
                /* scriptAnalysis      */ &analysis_result.analysis,
                /* localeName          */ locale,
                /* numberSubstitution  */ nullptr,
                /* features            */ nullptr,
                /* featureRangeLengths */ nullptr,
                /* featureRanges       */ 0,
                /* maxGlyphCount       */ static_cast<u32>(run.glyph_indices.size()),
                /* clusterMap          */ cluster_map.data(),
                /* textProps           */ text_props.data(),
                /* glyphIndices        */ run.glyph_indices.data() + total_glyph_count,
                /* glyphProps          */ glyph_props.data(),
                /* actualGlyphCount    */ &actual_glyph_count
            );

            if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && ++retry < 8) {
                estimated_glyph_count *= 2;
                estimated_glyph_count_next = total_glyph_count + estimated_glyph_count;
                run.glyph_indices.resize(estimated_glyph_count_next);
                glyph_props.resize(estimated_glyph_count);
                continue;
            }

            THROW_IF_FAILED(hr);
            break;
        }

        auto actual_glyph_count_next = total_glyph_count + actual_glyph_count;
        if (run.glyph_advances.size() < actual_glyph_count_next) {
            auto size = run.glyph_advances.size() * 2;
            size = std::max<size_t>(size, actual_glyph_count);
            run.glyph_advances.resize(size);
            run.glyph_advances.resize(size);
        }

        // Step 3: Get the glyph advances and the offsets relative to their target position.
        THROW_IF_FAILED(text_analyzer->GetGlyphPlacements(
            /* textString          */ text + analysis_result.text_position,
            /* clusterMap          */ cluster_map.data(),
            /* textProps           */ text_props.data(),
            /* textLength          */ analysis_result.text_length,
            /* glyphIndices        */ run.glyph_indices.data() + total_glyph_count,
            /* glyphProps          */ glyph_props.data(),
            /* glyphCount          */ actual_glyph_count,
            /* fontFace            */ font_face,
            /* fontEmSize          */ font_size,
            /* isSideways          */ false,
            /* isRightToLeft       */ 0,
            /* scriptAnalysis      */ &analysis_result.analysis,
            /* localeName          */ locale,
            /* features            */ nullptr,
            /* featureRangeLengths */ nullptr,
            /* featureRanges       */ 0,
            /* glyphAdvances       */ run.glyph_advances.data() + total_glyph_count,
            /* glyphOffsets        */ run.glyph_offsets.data() + total_glyph_count
        ));

        total_glyph_count = actual_glyph_count_next;
    }

    run.glyph_indices.resize(total_glyph_count);
    run.glyph_advances.resize(total_glyph_count);
    run.glyph_offsets.resize(total_glyph_count);
}

// This combines all of the above to map a piece of text to a series of DWRITE_GLYPH_RUNs.
static std::vector<OwnedGlyphRun>
dwrite_map_text_to_glyphs(IDWriteFontFallback1* font_fallback,
                          IDWriteFontCollection* font_collection,
                          IDWriteTextAnalyzer1* text_analyzer,
                          const wchar_t* locale, const wchar_t* base_family,
                          f32 font_size, const wchar_t* text, u32 text_length)
{
    std::vector<OwnedGlyphRun> runs;

    for (u32 fallback_offset = 0; fallback_offset < text_length;) {
        auto fallback = dwrite_map_characters(font_fallback, font_collection, locale, base_family, text, text_length, fallback_offset);

        // If no font contains the given codepoints MapCharacters() will return a nullptr font_face.
        // We need to replace them with ? glyphs, which this code doesn't do yet (by convention that's glyph index 0 in any font).
        if (!fallback.mapped_font_face) {
            continue;
        }

        auto fallback_beg = text + fallback_offset;
        auto fallback_remaining = fallback.mapped_length;
        auto& run = runs.emplace_back();

        while (fallback_remaining > 0) {
            const auto complexity = dwrite_map_text_complexity(text_analyzer, fallback.mapped_font_face.get(), fallback_beg, fallback_remaining);

            if (complexity.is_simple) {
                dwrite_map_glyphs_simple(fallback.mapped_font_face.get(), font_size, complexity.glyph_indices.data(), complexity.mapped_length, run);
            } else {
                dwrite_map_glyphs_complex(text_analyzer, locale, fallback.mapped_font_face.get(), font_size, fallback_beg, complexity.mapped_length, run);
            }

            fallback_beg += complexity.mapped_length;
            fallback_remaining -= complexity.mapped_length;
        }

        run.font_face = std::move(fallback.mapped_font_face);
        run.glyph_run.fontFace = run.font_face.get();
        run.glyph_run.fontEmSize = font_size;
        run.glyph_run.glyphCount = static_cast<u32>(run.glyph_indices.size());
        run.glyph_run.glyphIndices = run.glyph_indices.data();
        run.glyph_run.glyphAdvances = run.glyph_advances.data();
        run.glyph_run.glyphOffsets = run.glyph_offsets.data();

        fallback_offset += fallback.mapped_length;
    }

    return runs;
}
