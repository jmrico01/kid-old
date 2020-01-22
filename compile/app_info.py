from env_settings import WIN32_VCVARSALL

class LibExternal:
	def __init__(self, name, path, compiledNames = None, dllNames = None):
		self.name = name
		self.path = path
		self.compiledNames = compiledNames
		self.dllNames = dllNames

PROJECT_NAME = "kid"

COPY_DIRS = {
	"/data": "/data",
	"/src/shaders": "/shaders"
}

DEFINES = []

DEPLOY_FILES = [
	"data",
	"logs",
	"shaders",
	"kid_win32.exe"
]

LIBS_EXTERNAL = [
	LibExternal("freetype", "freetype-2.8.1", {
		"debug":   "freetype281MTd.lib",
		"release": "freetype281MT.lib"
	}),
	LibExternal("stbimage",   "stb_image-2.23"),
	LibExternal("stbsprintf", "stb_sprintf-1.06"),
	LibExternal("utf8proc",   "utf8proc")
]

PATHS = {
	"win32-vcvarsall": WIN32_VCVARSALL
}
