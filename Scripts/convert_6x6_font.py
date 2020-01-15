import json

rasterLinesPerGlyph = 12
skipGlyphHeaderLines = 6
glyphHeight = 6
glyphShiftRight = 2
minGlyphIndex = 32
maxGlyphIndex = 126
fontFileName = "Resources/6x6_font.json"
headerFileName = "Resources/6x6_font.h"


def convertToBinary(fontArray):
    glyph_data = []
    for raster_line in fontArray[skipGlyphHeaderLines:(skipGlyphHeaderLines + glyphHeight)]:
        binary_data = '{:08b}'.format(
            int(raster_line) >> glyphShiftRight)[::-1]
        glyph_data.append(f'0b{binary_data}')
        formatted_lines = ",\n         ".join(glyph_data)

    return f'    {{\n         {formatted_lines}\n    }},\n'


with open(fontFileName, 'r') as f:
    font_data = json.load(f)

font_code = {}
font_data["32"] = [0] * 12

for item in font_data:
    if (item.isdigit()):
        itemNum = int(item)
        if (itemNum >= minGlyphIndex and itemNum <= maxGlyphIndex):
            index = itemNum - minGlyphIndex
            font_code[index] = convertToBinary(font_data[item])

with open(headerFileName, 'w') as ff:
    glyph_count = len(font_code)
    ff.write(
        f'static const unsigned char FontData[{glyph_count}][{glyphHeight}] =\n')
    ff.write("{\n")

    for key in sorted(font_code.keys()):
        index = int(key) + minGlyphIndex
        ff.write(f'// Character {index}: \'{chr(index)}\'\n')
        ff.write(font_code[key])

    ff.write("};")
