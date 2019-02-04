import itertools
import os

import numpy
import png

JUMP_SEQUENCE_PATH = "./data/animations/kid/jump/"

def ExtractRootMotion(sequencePath):
	width = 0
	height = 0
	planes = 0
	numFrames = len(os.listdir(sequencePath))
	offsets = []

	imagePrev = None
	for i in range(numFrames):
		frameFilePath = os.path.join(sequencePath, "{}.png".format(i))
		print(frameFilePath)
		pngReader = png.Reader(filename=frameFilePath)
		pngData = pngReader.asDirect()
		assert pngData[3]["bitdepth"] == 8
		if i == 0:
			width = pngData[0]
			height = pngData[1]
			planes = pngData[3]["planes"]
		else:
			assert width == pngData[0]
			assert height == pngData[1]
			assert planes == pngData[3]["planes"]

		pixels = list(map(numpy.uint8, pngData[2]))
		image = numpy.vstack(pixels)
		if i > 0:
			minDiff = 1e6
			minOffset = 0
			OFFSET_RANGE = int(height / 3)
			for yOffset in range(-OFFSET_RANGE, OFFSET_RANGE):
				rolled = numpy.roll(imagePrev, shift=yOffset, axis=1)
				#print(rolled.shape)
				#print(imagePrev[0])
				#print(rolled[0])
				#break
				diff = numpy.average(numpy.square(image - rolled))
				if diff < minDiff:
					minDiff = diff
					minOffset = yOffset
			#print("min diff {} at offset {}".format(minDiff, minOffset))
			offsets.append(minOffset)

		imagePrev = image

	print("Frame {}x{}, {} planes".format(width, height, planes))
	print(offsets)

ExtractRootMotion(JUMP_SEQUENCE_PATH)