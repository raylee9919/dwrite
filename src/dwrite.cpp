// @NOTE:
// This function performs an iterative "font fallback" starting at text_offset.
// In most cases it'll simply look up the given base_family in the given font_collection and return that (= your primary font).
// But if the base_family doesn't have glyphs for the given text, it'll find another, alternative font and return that one.
// For instance, if the text contains emojis but your font doesn't have any, it'll return a font that does (= e.g. Segoe UI Emoji).
// DirectWrite implements 2 font fallback models:
// 'IDWriteFontFallback' implements the weight-stretch-style model.
// 'IDWriteFontFallback1' implements the more modern typographic model (Windows 10 RS3 and later).
static Dwrite_Map_Characters_Result
dwrite_map_characters(IDWriteFontFallback1 *font_fallback,
                      IDWriteFontCollection *base_font_collection,
                      WCHAR *locale, WCHAR *base_family, 
                      WCHAR *text, UINT32 text_length, UINT32 text_offset)
{
    Dwrite_Map_Characters_Result result = {};

    Dwrite_Text_Analysis_Source analysis_source = {locale, text, text_length};

    // @QUOTE: "I believe it's safe to ignore scale in practice." -lhecker
    FLOAT scale;

    HRESULT hr = font_fallback->MapCharacters(&analysis_source, text_offset, text_length,
                                              base_font_collection, base_family,
                                              nullptr/*fontAxisValues*/, 0/*fontAxisValueCount*/,
                                              &result.mapped_length, &scale, &result.mapped_font_face);

    assume(SUCCEEDED(hr));

    return result;
}


// @NOTE:
// 'IDWriteTextAnalyzer::GetTextComplexity()' will determine the longest
// run of characters that map 1:1 to glyphs without any ambiguity.
// If so, it'll return TRUE, and you can use the returned indices immediately.
// Otherwise, you must perform full glyph shaping.
static Text_Complexity_Result
dwrite_map_text_complexity(IDWriteTextAnalyzer1 *text_analyzer, IDWriteFontFace *font_face, WCHAR *text, UINT32 text_length)
{
    Text_Complexity_Result result = {};
    result.glyph_indices = (UINT16 *)malloc(sizeof(UINT16)*text_length);

    text_analyzer->GetTextComplexity(text, text_length, font_face, &result.is_simple, &result.mapped_length, result.glyph_indices);
    return result;
}


// @NOTE: Computes glyph advances.
static void
dwrite_map_glyphs_simple(IDWriteFontFace5 *font_face, FLOAT font_size,
                         UINT16 *indices, UINT32 index_count, Owned_Glyph_Run *run)
{
    DWRITE_FONT_METRICS1 metrics;
    font_face->GetMetrics(&metrics);

    // Retrives the advancs in design units for according glyph indices.
    INT32 *design_advances = new INT32[index_count];
    font_face->GetDesignGlyphAdvances(index_count, indices, design_advances, FALSE);

    UINT32 total_glyph_count = run->index_count;

    run->advances = new FLOAT[total_glyph_count + index_count];
    run->offsets = new DWRITE_GLYPH_OFFSET[total_glyph_count + index_count];

    run.glyph_indices.insert(run.glyph_indices.end(), indices, indices + indices_count);
    run->indices.insert(run->indices.end(), indices, indices + indices_count);
    run->advances.resize(total_glyph_count + indices_count);
    run->offsets.resize(total_glyph_count + indices_count);

    FLOAT scale = font_size / metrics.designUnitsPerEm;
    for (UINT32 i = 0; i < indices_count; i++) 
    {
        run->advances[total_glyph_count++] = design_advances[i] * scale;
    }
}


// This function performs proper (complex) glyph shaping.
// Since this function performs only the most simple form of glyph shaping (i.e. not justification, etc.), it requires just 3 steps.
static void
dwrite_map_glyphs_complex(IDWriteTextAnalyzer1 *analyzer,, WCHAR *locale,
                          IDWriteFontFace5 *font_face, FLOAT font_size, 
                          WCHAR *text, UINT32 text_length,
                          Owned_Glyph_Run *run)
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
dwrite_map_text_to_glyphs(IDWriteFontFallback1 *font_fallback,
                          IDWriteFontCollection *font_collection,
                          IDWriteTextAnalyzer1 *text_analyzer,
                          WCHAR *locale, WCHAR *base_family,
                          FLOAT font_size, WCHAR *text, UINT32 text_length)
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
