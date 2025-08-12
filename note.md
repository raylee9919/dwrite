# <span style="color: #ffa0a0">1. Types</span>

## 1-1. TrueType
Font tech designed by Apple, and now used in both MAC and Windows.
Includes information about how the characters should be spaced vertically and
horizontally within a block of text, character mapping details.
Character is defined by mathematical description of its outline.

## 1-2. ClearType
Microsoft's anti-aliasing technology which exploits LCD's vertical RGB stripes.
Grayscale is analogous to the traditional method which regards pixel as atomic, 
1/0 thing which can be turned on/off.

## 1-3. OpenType [[#]](https://learn.microsoft.com/en-us/windows/win32/intl/opentype-font-format)
Allows mapping between characters and glyphs, enabling support for ligatures...

## 1-4. TTF vs OTF
OTF stands for Postscript-flavored <span style="color: #a0a0ff">OpenType</span>.<br>
TTF stands for Truetype-flavored <span style="color: #a0a0ff">OpenType</span>.<br><br>
OTF uses cubic curves while TTF uses quadratic curves. It mattered in old days when printers were more limited.<br>
TTF uses full-on hinting, which is much more involved than the zones in OTF.<br><br>
Mac ignores hinting entirely and renders all fonts the same.<br>
Windows doesn't assume that all fonts should be rendered the same. i.e., it relies on hinting.<br>
Since OTF doesn't go full on hinting, occasionally, OTF seemes less better than in Windows than in Mac.<br>
But, OTF is simpler and easier to get out the door for designers.


# <span style="color: #ffa0a0">2. DirectWrite</span> [[#]](https://learn.microsoft.com/en-us/typography/cleartype/)
DirectWrite is independent of any particular graphics technology: GDI, D3D, D2D...

There are two APIs for rendering glyphs:<br>
1. Direct2D
    Hardware accerlated.
    `ID2DRenderTarget::DrawGlyphRun()`
2. GDI Bitmap
    Software rendering.
    `IDWriteBitmapRenderTarget::DrawGlyphRun()`


## 2-1. Rendering Modes
`IDWriteRenderingParams` encapsulates rendering parameters:<br>
Generally, is you use DWrite layout API, rendering mode is automatically selected.<br>
But the user can call `IDWriteFactory::CreateCustomRenderingParams` for more control.<br>

## 2-2. DWriteCore [[#]](https://learn.microsoft.com/en-us/windows/win32/directwrite/dwritecore-overview)
It is part of the Windows SDK which isnt' part of the OS.<br>
> "It can rasterize emojis on its own while DWrite can't." -lhecker
>

## 2-3. Text Formatting and Layout [[#]](https://learn.microsoft.com/en-us/windows/win32/directwrite/text-formatting-and-layout)

### 2-3-a. IDWriteTextFormat
Describe the foramt for an entire string when rendering.<br>
Call `IDWriteFactory::CreateTextFormat()` to create `IDWriteTextFormat` and specify formats.<br>
Most of the attributes are immutable once created.

### 2-3-b. IDWriteTextLayout
The text is immutable.
> "Since font fallback and glyph shaping are expensive, 
> IDWriteTextLayout is supposed to be a convenient way to cache that.
> That allows you to repeatedly draw the same string without the big CPU cost." -lhecker
>

# <span style="color: #ffa0a0">3. Glyph</span>

## 3-1. Glyph
<span style="color: #ffa0a0">**:=**</span> Physical representation of a character in given font.

## 3-2. Glyph Run
<span style="color: #ffa0a0">**:=**</span> A contiguous set of glyphs that all have the same font face, size, and client drawing effect.

## 3-3. FontFace
<span style="color: #ffa0a0">**:=**</span> Represents a physical font, with a specific *weight*, *slant*, and *stretch*.<br>
`IDWriteFontFace` can be created directly from a font name or obtained from a font collection.

## 3-4. Glyph Metrics
Each glyph has its own metric.<br>
`IDWriteFontFace::GetDesignGlyphMetrics` gives metrics for all glyphs.

# <span style="color: #ffa0a0">4. API</span>

## 4-1. IDWriteGlyphRunAnalysis
DWrite and D2D rasterization uses it under the hood.<br>
Chromium and skia uses uses it for rasterization.

## 4-1. IDWriteTextLayout
Font fallback and glyph shaping are costly.<br>
It is supposed to be convenient way to cache that.<br>
You repeatedly draw the same thing without the big CPU cost.<br>
`IDWriteFontFallback::MapCharacters` and `IDWriteTextAnalyzer::GetGlyphs` are fairly expensive calls.
