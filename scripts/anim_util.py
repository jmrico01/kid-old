import itertools
import os

import numpy
import png

JUMP_SEQUENCE_PATH = "./data/animations/kid/jump-rootmotion/"

IMG_CHANNELS = 4
IMG_BITDEPTH = 8

def ExtractRootMotion(sequencePath):
	width = 0
	height = 0
	planes = 0
	numFrames = len(os.listdir(sequencePath))
	positions = []

	for i in range(numFrames):
		frameFilePath = os.path.join(sequencePath, "{}.png".format(i))
		print(frameFilePath)
		pngReader = png.Reader(filename=frameFilePath)
		pngData = pngReader.asDirect()
		assert pngData[3]["bitdepth"] == IMG_BITDEPTH
		assert pngData[3]["planes"] == IMG_CHANNELS
		if i == 0:
			width = pngData[0]
			height = pngData[1]
		else:
			assert width == pngData[0]
			assert height == pngData[1]

		pixels = list(pngData[2])
		for y in range(height):
			row = pixels[y]
			for x in range(width):
				ind = x * IMG_CHANNELS
				rgba = row[ind:ind+4]
				if rgba == bytearray(b"\xff\x00\x00\xff"):
					positions.append((x, y))

	print("Frame {}x{}, {} planes".format(width, height, planes))
	print(positions)

ExtractRootMotion(JUMP_SEQUENCE_PATH)