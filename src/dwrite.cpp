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
    UINT32 index_cap = text_length;
    UINT16 *indices = NULL;
    arrsetcap(indices, index_cap);

    HRESULT hr = text_analyzer->GetTextComplexity(text, text_length, font_face,
                                                  /* out */
                                                  &is_simple, &mapped_length, indices);
    assume(SUCCEEDED(hr));

    result.glyph_indices = indices;
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

    HRESULT hr = S_OK;

    UINT32 offset = 0;
    while (offset < text_length)
    {
        UINT32 mapped_length;
        IDWriteFontFace5 *mapped_font_face = NULL;
        { // Perform fallback and acquire the run of identical font face. -> (fontface, offset, len)
            FLOAT dummy_scale; // @Quote(lhecker): It's safe to ignore scale in practice.

            Dwrite_Text_Analysis_Source src = {locale, text, text_length};
            font_fallback->MapCharacters(&src, offset, text_length, font_collection, base_family,
                                         NULL, 0, &mapped_length, &dummy_scale, &mapped_font_face);

            // @Todo: If no font contains the given codepoints MapCharacters() will return a NULL font_face.
            // We need to replace them with ? glyphs, which this code doesn't do yet (by convention that's glyph index 0 in any font).
            assume(mapped_font_face);
        }

        Dwrite_Glyph_Run run = {};
        run.font_face = mapped_font_face;

        // Once our string is segmented to runs of identical font face,
        // we must now segment those once again into runs of same complexity.
        const WCHAR *remain_txt = text + offset;
        UINT32 remain_len = mapped_length;

        while (remain_len > 0)
        {
            Dwrite_Map_Complexity_Result complexity = dwrite_map_complexity(text_analyzer, mapped_font_face, remain_txt, remain_len);

            if (complexity.is_simple)
            {
                UINT32 glyph_count_add = complexity.mapped_length;
                UINT32 glyph_count_old = arrlenu(run.indices);
                UINT32 glyph_count_new = glyph_count_old + glyph_count_add;

                arrsetlen(run.indices,  glyph_count_new);
                arrsetlen(run.advances, glyph_count_new);
                arrsetlen(run.offsets,  glyph_count_new);

                INT32 *design_advances = NULL;
                {
                    arrsetlen(design_advances, glyph_count_add);
                    hr = mapped_font_face->GetDesignGlyphAdvances(glyph_count_add, complexity.glyph_indices, design_advances, FALSE /*RetrieveVerticalAdvance*/);
                    assume(SUCCEEDED(hr));
                }

                DWRITE_FONT_METRICS1 metrics;
                mapped_font_face->GetMetrics(&metrics);

                FLOAT pt_per_design_unit = font_size / metrics.designUnitsPerEm;
                for (UINT i = 0; i < glyph_count_add; ++i)
                {
                    UINT idx = glyph_count_old + i;
                    run.indices[idx]  = complexity.glyph_indices[i];
                    run.advances[idx] = design_advances[i] * pt_per_design_unit;
                    run.offsets[idx]  = {};
                }
            }
            else
            {
                const WCHAR *text = remain_txt;
                const UINT32 text_length = complexity.mapped_length;

                // @Note: Split the text into runs of the same script ("language"), bidi, etc.
                Dwrite_Text_Analysis_Sink analysis_sink = {};
                Dwrite_Text_Analysis_Source analysis_source = {locale, text, text_length};
                hr = text_analyzer->AnalyzeScript(&analysis_source, 0/*textPosition*/, text_length, &analysis_sink);
                assume(SUCCEEDED(hr));

                for (UINT32 i = 0; i < arrlenu(analysis_sink.results); ++i)
                {
                    Dwrite_Text_Analysis_Sink_Result analysis_sink_result = analysis_sink.results[i];

                    UINT16 *cluster_map = NULL;
                    DWRITE_SHAPING_TEXT_PROPERTIES *text_props = NULL;
                    DWRITE_SHAPING_GLYPH_PROPERTIES *glyph_props = NULL;

                    UINT32 estimated_additional_glyph_count = (3 * analysis_sink_result.text_length / 2 + 16);
                    UINT32 current_glyph_count = arrlenu(run.indices);
                    UINT32 estimated_glyph_count = current_glyph_count + estimated_additional_glyph_count;

                    if (arrlenu(cluster_map) < analysis_sink_result.text_length)
                    {
                        arrsetlen(cluster_map, analysis_sink_result.text_length);
                        arrsetlen(text_props, analysis_sink_result.text_length);
                    }

                    UINT32 actual_additional_glyph_count = 0; 

                    UINT32 retry_count = 0;
                    while (retry_count < 4)
                    {
                        if (arrlenu(run.indices) < estimated_glyph_count)
                        { arrsetlen(run.indices, estimated_glyph_count); }

                        if (arrlenu(glyph_props) < estimated_glyph_count)
                        { arrsetlen(glyph_props, estimated_glyph_count); }


                        hr = text_analyzer->GetGlyphs(text + analysis_sink_result.text_position,
                                                      analysis_sink_result.text_length,
                                                      mapped_font_face,
                                                      FALSE,                       // isSideways
                                                      FALSE,                       // isRightToLeft,
                                                      &analysis_sink_result.analysis,
                                                      locale,
                                                      NULL,                        // numberSubstitution,
                                                      NULL,                        // features
                                                      NULL,                        // featureRangeLengths
                                                      0,                           // featureRanges
                                                      estimated_additional_glyph_count,

                                                      /* Out */
                                                      cluster_map,
                                                      text_props,
                                                      run.indices + current_glyph_count,
                                                      glyph_props,
                                                      &actual_additional_glyph_count);

                        if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
                        {
                            estimated_additional_glyph_count *= 2;
                            estimated_glyph_count = current_glyph_count + estimated_additional_glyph_count;
                            retry_count++;
                        }
                        else
                        {
                            break;
                        }
                    }


                    UINT32 actual_new_glyph_count = current_glyph_count + actual_additional_glyph_count;
                    arrsetlen(run.indices, actual_new_glyph_count);
                    arrsetlen(run.advances, actual_new_glyph_count);
                    arrsetlen(run.offsets, actual_new_glyph_count);

                    for (UINT32 i = 0; i < actual_additional_glyph_count; ++i)
                    {
                        run.offsets[current_glyph_count + i] = {}; 
                    }

                    hr = text_analyzer->GetGlyphPlacements(text + analysis_sink_result.text_position,
                                                           cluster_map,
                                                           text_props,
                                                           analysis_sink_result.text_length,
                                                           run.indices + current_glyph_count,
                                                           glyph_props,
                                                           actual_additional_glyph_count,
                                                           mapped_font_face,
                                                           font_size,
                                                           FALSE,                               // isSideways
                                                           FALSE,                               // isRightToLeft
                                                           &analysis_sink_result.analysis,
                                                           locale,
                                                           NULL,                                // features
                                                           NULL,                                // featureRangeLengths
                                                           0,                                   // featureRanges

                                                           /* out */
                                                           run.advances + current_glyph_count,
                                                           run.offsets + current_glyph_count);
                }
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
