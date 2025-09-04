// @Note: Determines the longest run of characters that map 1:1 to glyphs without
// ambiguity. In that case, it returns TRUE and you can immediately use indices.
// Otherwise, perform full glyph shaping.
static Dwrite_Map_Complexity_Result
dwrite_map_complexity(IDWriteTextAnalyzer1 *text_analyzer,
                      IDWriteFontFace *font_face,
                      WCHAR *text, U32 text_length)
{
    Dwrite_Map_Complexity_Result result = {};

    B32 is_simple;
    U32 mapped_length;
    U32 index_cap = text_length;
    U16 *_indices = NULL;
    arrsetcap(_indices, index_cap);

    HRESULT hr = text_analyzer->GetTextComplexity(text, text_length, font_face,
                                                  /* out */
                                                  &is_simple, &mapped_length, _indices);
    assume(SUCCEEDED(hr));

    result.glyph_indices = _indices;
    result.is_simple     = is_simple;
    result.mapped_length = mapped_length;

    return result;
}

static Dwrite_Font_Fallback_Result
dwrite_font_fallback(IDWriteFontFallback1 *font_fallback,
                         IDWriteFontCollection *font_collection,
                         WCHAR *base_family, WCHAR *locale,
                         WCHAR *text, U32 text_length)
{
    Dwrite_Font_Fallback_Result result = {};

    // @Note: It's safe to ignore scale in practice. -lhecker
    FLOAT dummy_scale;

    Dwrite_Text_Analysis_Source src = {locale, text, text_length};
    font_fallback->MapCharacters(&src, 0/*offset*/, text_length, font_collection, base_family,
                                 NULL/*fontAxisValues*/, 0/*fontAxisValueCount*/,
                                 /* out */
                                 &result.length, &dummy_scale, &result.font_face);

    // @Todo: If no font contains the given codepoints MapCharacters() will return a NULL font_face.
    // We need to replace them with ? glyphs, which this code doesn't do yet (by convention that's glyph index 0 in any font).
    assume(result.font_face);

    return result;
}

static DWRITE_GLYPH_RUN *
dwrite_map_text_to_glyphs(IDWriteFontFallback1 *font_fallback,
                          IDWriteFontCollection *font_collection,
                          IDWriteTextAnalyzer1 *text_analyzer,
                          WCHAR *locale, WCHAR *base_family,
                          FLOAT pt_per_em, WCHAR *text, U32 text_length)
{
    DWRITE_GLYPH_RUN *result = NULL;
    F32 max_advance_height_px = 0.0f; // @Todo: return this

    HRESULT hr = S_OK;

    U32 offset = 0;
    while (offset < text_length)
    {
        Dwrite_Font_Fallback_Result ff = dwrite_font_fallback(font_fallback, font_collection, base_family, locale,
                                                              text + offset, text_length - offset);
        U32 run_length = ff.length;
        IDWriteFontFace5 *run_font_face = ff.font_face;
        assert(run_font_face);

        DWRITE_FONT_METRICS dfm = {};
        run_font_face->GetMetrics(&dfm);
        F32 du_per_em = dfm.designUnitsPerEm;
        F32 em_per_du = 1.0f / (F32)du_per_em;
        F32 px_per_em = pt_per_em*1.333333f;
        F32 px_per_du = px_per_em * em_per_du;

        F32 advance_height_px = (F32)(dfm.ascent + dfm.descent + dfm.lineGap) * px_per_du;
        max_advance_height_px = max(max_advance_height_px, advance_height_px);

        // --------------------------------------------------------------------
        // @Note: Put to hash table.
        // @Important: Must not free a font face for this to work.
        if (hmgeti(dwrite_font_hash_table, (U64)run_font_face) == -1) // !exists
        {
            Dwrite_Font_Metrics metrics = {};
            metrics.du_per_em = du_per_em;
            metrics.advance_height_px = advance_height_px;
            hmput(dwrite_font_hash_table, (U64)run_font_face, metrics);
        }

        U16 *indices                 = NULL;
        FLOAT *advances              = NULL;
        DWRITE_GLYPH_OFFSET *offsets = NULL;

        // Segment the run once again with identical complexity.
        WCHAR *remain_text = text + offset;
        U32 remain_length = run_length;
        while (remain_length)
        {
            Dwrite_Map_Complexity_Result complexity = dwrite_map_complexity(text_analyzer, run_font_face, remain_text, remain_length);

            
            if (complexity.is_simple)
            {
                U32 glyph_count_add = complexity.mapped_length;
                U32 glyph_count_old = (U32)arrlenu(indices);
                U32 glyph_count_new = glyph_count_old + glyph_count_add;

                arrsetlen(indices,  glyph_count_new);
                arrsetlen(advances, glyph_count_new);
                arrsetlen(offsets,  glyph_count_new);

                S32 *advances_du = NULL;
                arrsetlen(advances_du, glyph_count_add);
                run_font_face->GetDesignGlyphAdvances(glyph_count_add, complexity.glyph_indices, advances_du, FALSE /*RetrieveVerticalAdvance*/);

                for (U32 i = 0; i < glyph_count_add; ++i)
                {
                    U32 idx = glyph_count_old + i;
                    indices[idx]  = complexity.glyph_indices[i];
                    advances[idx] = advances_du[i] * px_per_em * em_per_du; // @Todo: Unit?
                    offsets[idx]  = {};
                }
            }
            else // complex
            {
                U32 text_length = complexity.mapped_length;

                Dwrite_Text_Analysis_Source analysis_source = {locale, remain_text, text_length};
                Dwrite_Text_Analysis_Sink analysis_sink = {};
                U16 *cluster_map = NULL;
                DWRITE_SHAPING_TEXT_PROPERTIES *text_props = NULL;
                DWRITE_SHAPING_GLYPH_PROPERTIES *glyph_props = NULL;

                U32 current_glyph_count = (U32)arrlenu(indices);
                U32 estimated_glyph_count_final = current_glyph_count + (3 * text_length) / 2 + 16;
                arrsetlen(indices, estimated_glyph_count_final);
                arrsetlen(advances, estimated_glyph_count_final);
                arrsetlen(offsets, estimated_glyph_count_final);

                // Split the text into runs of the same script ("language"), bidi, etc.
                hr = text_analyzer->AnalyzeScript(&analysis_source, 0/*textPosition*/, text_length, &analysis_sink);
                assume(SUCCEEDED(hr));

                for (U32 i = 0; i < arrlenu(analysis_sink.results); ++i)
                {
                    Dwrite_Text_Analysis_Sink_Result analysis_sink_result = analysis_sink.results[i];

                    U32 estimated_glyph_count_add = (3 * analysis_sink_result.text_length / 2 + 16);
                    U32 estimated_glyph_count_next = current_glyph_count + estimated_glyph_count_add;

                    if (arrlenu(cluster_map) < analysis_sink_result.text_length)
                    {
                        arrsetlen(cluster_map, analysis_sink_result.text_length);
                        arrsetlen(text_props, analysis_sink_result.text_length);
                    }

                    if (arrlenu(indices) < estimated_glyph_count_next)
                    { arrsetlen(indices, estimated_glyph_count_next); }

                    if (arrlenu(glyph_props) < estimated_glyph_count_next)
                    { arrsetlen(glyph_props, estimated_glyph_count_next); }

                    U32 actual_glyph_count_add = 0; 

                    U32 retry_count = 0;
                    while (retry_count < 8)
                    {
                        hr = text_analyzer->GetGlyphs(remain_text + analysis_sink_result.text_position,
                                                      analysis_sink_result.text_length,
                                                      run_font_face,
                                                      FALSE,                       // isSideways
                                                      0,                           // isRightToLeft,
                                                      &analysis_sink_result.analysis,
                                                      locale,
                                                      NULL,                        // numberSubstitution,
                                                      NULL,                        // features
                                                      NULL,                        // featureRangeLengths
                                                      0,                           // featureRanges
                                                      (U32)arrlenu(indices),

                                                      /* Out */
                                                      cluster_map,
                                                      text_props,
                                                      indices + current_glyph_count,
                                                      glyph_props,
                                                      &actual_glyph_count_add);

                        if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
                        {
                            estimated_glyph_count_add *= 2;
                            estimated_glyph_count_next = current_glyph_count + estimated_glyph_count_add;
                            arrsetlen(indices, estimated_glyph_count_next);
                            arrsetlen(glyph_props, estimated_glyph_count_add);
                            retry_count++;
                        }
                        else if (FAILED(hr))
                        {
                            assume(! "x");
                        }
                        else
                        {
                            break;
                        }
                    }

                    U32 actual_glyph_count_next = current_glyph_count + actual_glyph_count_add;
                    if (arrlenu(advances) < actual_glyph_count_next)
                    {
                        U64 size = (arrlenu(advances) << 1);
                        size = max(size, actual_glyph_count_add);
                        arrsetlen(advances, size);
                    }

                    hr = text_analyzer->GetGlyphPlacements(remain_text + analysis_sink_result.text_position,
                                                           cluster_map,
                                                           text_props,
                                                           analysis_sink_result.text_length,
                                                           indices + current_glyph_count,
                                                           glyph_props,
                                                           actual_glyph_count_add,
                                                           run_font_face,
                                                           px_per_em,
                                                           FALSE, // isSideways
                                                           0,     // isRightToLeft
                                                           &analysis_sink_result.analysis,
                                                           locale,
                                                           NULL,  // features
                                                           NULL,  // featureRangeLengths
                                                           0,     // featureRanges

                                                           /* out */
                                                           advances + current_glyph_count, // @Todo: Unit consistency.
                                                           offsets + current_glyph_count);

                    assume(SUCCEEDED(hr));

                    current_glyph_count = actual_glyph_count_next;
                }

                arrsetlen(indices, current_glyph_count);
                arrsetlen(advances, current_glyph_count);
                arrsetlen(offsets, current_glyph_count);
            }

            remain_text += complexity.mapped_length;
            remain_length -= complexity.mapped_length;
        }

        DWRITE_GLYPH_RUN run = {};
        run.fontFace      = run_font_face;
        run.fontEmSize    = px_per_em;
        run.glyphCount    = (U32)arrlenu(indices);
        run.glyphIndices  = indices;
        run.glyphAdvances = advances;
        run.glyphOffsets  = offsets;
        arrput(result, run);

        offset += run_length;
    }

    return result;
}
