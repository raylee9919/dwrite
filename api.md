# Typography

## Unit
- Pixel<br>
- Dot<br>
= Pixel (in current context)
- Inch<br>
1″ = 2.54cm
- Point<br>
Designed by *Pierre Simon Fournier*.<br>
1pt= 0.13837″ ≈ **0.72″**<br>
<br>
Thus, given the 72pt font size on 96DPI monitor, convert to pixel size.


## DPI
In the past, most displays ahd 96 pixels per linear inch of physical space (96-DPI).
In 2017, displays with nearly 300-DPI or higher are available.

## Font
A collection of characters and symbols that share a common design. The three 
major elements of this design are referred to as **typeface**, **style**, and **size**.
  1. Typeface<br>
  2. Offset<br>
  Refers to the **weight** and **slant** of a font.<br>
  3. Size<br>
  Font's size is specified in points.




# API

## DPI
You can set the default DPI awareness programmatically although MSDN says it's not recommended.

## IDWriteFontFace::GetMetrics()
**Metrics** help the computer determine things like the default spacing 
between lines, how high or low sub and super scripts should go...<br>
The default vertical advance should be the sum of `ascent + descent + lineGap`
(but converted from design units to pixel).
