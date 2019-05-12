import psd_tools

def ConvertPSD(filePath):
	psd = psd_tools.PSDImage.open(filePath)
	print(psd.width)
	print(psd.height)

	for layer in psd:
		print(layer)

ConvertPSD("misc-scripts/file.psd")
