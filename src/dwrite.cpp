// @Note: Determines the longest run of characters that map 1:1 to glyphs without
// ambiguity. In that case, it returns TRUE and you can immediately use indices.
// Otherwise, perform full glyph shaping.
static Dwrite_Map_Complexity_Result
dwrite_map_complexity(IDWriteTextAnalyzer1 *text_analyzer,
                           IDWriteFontFace *font_face,
                           const WCHAR *text, UINT32 text_length)
{
    Dwrite_Map_Complexity_Result result = {};

    BOOL is_simple;
    UINT32 mapped_length;
    UINT32 index_count = text_length;
    UINT16 *indices = new UINT16[index_count];

    HRESULT hr = text_analyzer->GetTextComplexity(text, text_length, font_face,
                                                  &is_simple, &mapped_length, indices);
    assume(SUCCEEDED(hr));

    result.glyph_indices = indices;
    result.index_count   = index_count;
    result.is_simple     = is_simple;
    result.mapped_length = mapped_length;

    return result;
}

static Dwrite_Glyph_Run *
dwrite_map_text_to_glyphs(IDWriteFontFallback1 *font_fallback,
                          IDWriteFontCollection *font_collection,
                          IDWriteTextAnalyzer1 *text_analyzer,
                          const WCHAR *locale, const WCHAR *base_family,
                          FLOAT font_size, const WCHAR *text, UINT32 text_length)
{
    Dwrite_Glyph_Run *result = NULL;

    UINT32 offset = 0;
    while (offset < text_length)
    {
        UINT32 mapped_length;
        IDWriteFontFace5 *mapped_font_face;
        { // Perform fallback and acquire the run of identical font face. -> (fontface, offset, len)
            FLOAT scale; // @Quote(lhecker): It's safe to ignore scale in practice.

            Dwrite_Text_Analysis_Source src = {locale, text, text_length};
            font_fallback->MapCharacters(&src, offset, text_length, font_collection, base_family,
                                         NULL, 0, &mapped_length, &scale, &mapped_font_face);

            // @Todo: If no font contains the given codepoints MapCharacters() will return a NULL font_face.
            // We need to replace them with ? glyphs, which this code doesn't do yet (by convention that's glyph index 0 in any font).
            assume(mapped_font_face);
        }

        Dwrite_Glyph_Run run = {};

        // Once our string is segmented to runs of identical font face,
        // we must now segment those once again into runs of same complexity.
        const WCHAR *remain_txt = text + offset;
        UINT32 remain_len = mapped_length;

        while (remain_len > 0)
        {
            Dwrite_Map_Complexity_Result complexity = dwrite_map_complexity(text_analyzer, mapped_font_face, remain_txt, remain_len);

            if (complexity.is_simple)
            {
                UINT32 glyph_count_add = complexity.index_count;
                UINT32 glyph_count_old = arrlenu(run.indices);
                UINT32 glyph_count_new = glyph_count_old + glyph_count_add;

                arrsetlen(run.indices,  glyph_count_new);
                arrsetlen(run.advances, glyph_count_new);
                arrsetlen(run.offsets,  glyph_count_new);

                INT32 *design_advances = NULL;
                {
                    arrsetlen(design_advances, glyph_count_add);
                    HRESULT hr = mapped_font_face->GetDesignGlyphAdvances(glyph_count_add, complexity.glyph_indices, design_advances, FALSE /*RetrieveVerticalAdvance*/);
                    assume(SUCCEEDED(hr));
                }

                DWRITE_FONT_METRICS1 metrics;
                mapped_font_face->GetMetrics(&metrics);

                FLOAT scale = font_size / metrics.designUnitsPerEm;
                for (UINT i = 0; i < glyph_count_add; ++i)
                {
                    UINT idx = glyph_count_old + i;
                    run.indices[idx]  = complexity.glyph_indices[i];
                    run.advances[idx] = design_advances[i] * scale;
                    run.offsets[idx]  = {};
                }
            }
            else
            {
                //UINT32 length_estimate = (3 * length_text / 2 + 16);
                UINT32 glyph_count_max = ;
                UINT32 glyph_count_actual;

                UINT16 *cluster_map = NULL;
                DWRITE_SHAPING_TEXT_PROPERTIES *text_props = NULL;
                UINT16 *glyph_indices = NULL;
                DWRITE_SHAPING_GLYPH_PROPERTIES *glyph_props = NULL;

                hr = GetGlyphs(remain_txt,
                               /* in */
                               UINT32 textLength,
                               font_face,
                               FALSE, // isSideways
                               FALSE, // isRightToLeft,
                               [in]           DWRITE_SCRIPT_ANALYSIS const      *scriptAnalysis,
                               locale,
                               NULL, //numberSubstitution,
                               NULL, // features
                               NULL, // featureRangeLengths
                               0,    // featureRanges
                               max_glyph_count,

                               /* out */
                               cluster_map, text_props, glyph_indices, glyph_props,
                               &glyph_count_actual);

                if (HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
                {
                }
                else
                {
                }
                

#if 0
                // ??
                Dwrite_Text_Analysis_Source analysis_source{locale, remain_txt, remain_len};

                Dwrite_Text_Analysis_Sink analysis_sink;

                // ??
                UINT16 *cluster_map;
                DWRITE_SHAPING_TEXT_PROPERTIES *text_props;
                DWRITE_SHAPING_GLYPH_PROPERTIES *glyph_props;

                // @Note: This equation comes from the GetGlyphs() documentation.
                UINT64 glyph_count_current = arrlenu(run.indices);
                UINT64 glyph_count_estimate = glyph_count_current + (3 * text_length) / 2 + 16;
                arrsetlen(run.indices, glyph_count_estimate);
                arrsetlen(run.advances, glyph_count_estimate);
                arrsetlen(run.offsets, glyph_count_estimate);

                // @Note: [1] Split the text into runs of the same script ("language"), bidi, etc.
                // This only performs the absolute minimum with AnalyzeScript() which analyzes a text range for script boundaries.
                hr = text_analyzer->AnalyzeScript(&analysis_source,
                                                  0, // textPosition
                                                  remain_len, // textLength ??????????????
                                                  &analysis_sink);
                assume(SUCCEEDED(hr));

                for (UINT32 i = 0; i < arrlenu(analysis_sink.results); ++i)
                {
                    Dwrite_Text_Analysis_Sink_Result analysis_result = analysis_sink.results[i];

                    // @Note: This equation comes from the GetGlyphs() documentation.
                    auto estimated_glyph_count = (3 * analysis_result.text_length) / 2 + 16;
                    auto estimated_glyph_count_next = total_glyph_count + estimated_glyph_count;

                    // @Note: Fulfill the "_Out_writes_(...)" requirements of GetGlyphs().
                    if (cluster_map.size() < analysis_result.text_length) 
                    {
                        cluster_map.resize(analysis_result.text_length);
                        text_props.resize(analysis_result.text_length);
                    }
                    if (run.glyph_indices.size() < estimated_glyph_count_next) 
                    {
                        run.glyph_indices.resize(estimated_glyph_count_next);
                    }
                    if (glyph_props.size() < estimated_glyph_count) 
                    {
                        glyph_props.resize(estimated_glyph_count);
                    }

                    UINT32 actual_glyph_count = 0;

                    // @Note: [2] Map the given text into glyph indices.
                    for (UINT32 retry = 0;;) 
                    {
                        hr = text_analyzer->GetGlyphs(text + analysis_result.text_position,        // textString
                                                      analysis_result.text_length,                 // textLength
                                                      font_face,                                   // fontFace
                                                      FALSE, // isSideways
                                                      0, // isRightToLeft
                                                      &analysis_result.analysis,                   // scriptAnalysis
                                                      locale,
                                                      NULL, // numberSubstitution
                                                      NULL, // features
                                                      NULL, // featureRangeLengths
                                                      0, // featureRanges
                                                      arrlenu(run.indices),  // maxGlyphCount
                                                      cluster_map.data(),                          // clusterMap
                                                      text_props.data(),                           // textProps
                                                      run.glyph_indices.data() + total_glyph_count,// glyphIndices
                                                      glyph_props.data(),                          // glyphProps
                                                      &actual_glyph_count                          // actualGlyphCount
                                                     );

                        if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && ++retry < 8) 
                        {
                            estimated_glyph_count <<= 1;
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
                                                                      /* features            */ NULL,
                                                                      /* featureRangeLengths */ NULL,
                                                                      /* featureRanges       */ 0,
                                                                      /* glyphAdvances       */ run.glyph_advances.data() + total_glyph_count,
                                                                      /* glyphOffsets        */ run.glyph_offsets.data() + total_glyph_count
                                                                     ));

                    total_glyph_count = actual_glyph_count_next;
                }

                arrsetlen(run.indices, total_glyph_count);
                arrsetlen(run.advances, total_glyph_count;
                arrsetlen(run.offsets, total_glyph_count);
#endif
            }

            remain_txt += complexity.mapped_length;
            remain_len -= complexity.mapped_length;
        }

        run.run.fontFace       = mapped_font_face;
        run.run.fontEmSize     = font_size;
        run.run.glyphCount     = arrlenu(run.indices);
        run.run.glyphIndices   = run.indices;
        run.run.glyphAdvances  = run.advances;
        run.run.glyphOffsets   = run.offsets;

        // Append to the list of runs.
        arrput(result, run);
        offset += mapped_length;
    }
    
    return result;
}
