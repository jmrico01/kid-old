import itertools
import os

import numpy
import png

IMG_CHANNELS = 4
IMG_BITDEPTH = 8
IMG_COLOR_VALUES = 2 ** IMG_BITDEPTH

LUT_SIZE = 4096
QUADS = int(LUT_SIZE / IMG_COLOR_VALUES)

print("Generating base LUT")
print("  size: {}x{}, quads: {}".format(LUT_SIZE, LUT_SIZE, QUADS))

imgData = []

for y in range(LUT_SIZE):
    row = []
    for x in range(LUT_SIZE):
        quadX = int(x / IMG_COLOR_VALUES)
        quadY = int(y / IMG_COLOR_VALUES)
        if quadX < 0 or quadX >= QUADS or quadY < 0 or quadY >= QUADS:
            raise Exception("bad quad")

        red = x % IMG_COLOR_VALUES
        green = y % IMG_COLOR_VALUES
        blue = quadY * QUADS + quadX
        if red < 0 or red > 255:
            raise Exception("bad red: {}, ({}, {})".format(red, x, y))
        if green < 0 or green > 255:
            raise Exception("bad green: {}, ({}, {})".format(green, x, y))
        if blue < 0 or blue > 255:
            raise Exception("bad blue: {}, ({}, {})".format(blue, x, y))
        pixel = (red, green, blue, 255)
        row.append(pixel)

    imgData.append(row)

png.from_array(imgData, "RGBA").save("lutbase.png")