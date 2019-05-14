import os
import sys

import psd_tools

def ConvertPSD(filePath, outputDir):
	psd = psd_tools.PSDImage.open(filePath)
	print("Converting PSD file {} ({} x {})".format(filePath, psd.width, psd.height))
	print("")
	metadata = "size {} {}\n".format(psd.width, psd.height)

	print("Processing layers...")
	for layer in psd:
		layerName = layer.name
		layerOffset = layer.offset
		layerSize = layer.size
		layerImagePath = os.path.join(outputDir, layerName + ".png")
		image = layer.compose()
		assert layerSize == image.size
		image.save(layerImagePath)
		print("Layer {}, offset {}, size {}".format(layerName, layerOffset, layerSize))
		print("{}".format(layerImagePath))

		metadata += "\n"
		metadata += "name {}\n".format(layerName)
		offsetY = psd.height - layerSize[1] - layerOffset[1]
		metadata += "offset {} {}\n".format(layerOffset[0], offsetY)
	print("")

	metadataFilePath = os.path.join(outputDir, "sprites.kml")
	print("Writing sprite metadata to {}".format(metadataFilePath))
	print("")
	with open(metadataFilePath, "w") as metadataFile:
		metadataFile.write(metadata)

	print("Done!")

if len(sys.argv) != 3:
	print("Expected 2 arguments: input PSD, output dir")

psdFile = sys.argv[1]
outputDir = sys.argv[2]
ConvertPSD(psdFile, outputDir)
