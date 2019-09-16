from env_settings import WIN32_VCVARSALL

PROJECT_NAME = "kid"

DEPLOY_FILES = [
	"data",
	"logs",
	"shaders",
	"kid_win32.exe"
]

LIBS_EXTERNAL = {
	"freetype": {
		"path": "freetype-2.8.1",
		"includeDir": True,
		"compiled": True,
		"compiledName": {
			"debug": "freetype281MTd.lib",
			"release": "freetype281MT.lib"
		}
	},
	"stbimage": {
		"path": "stb_image-2.23",
		"includeDir": False,
		"compiled": False
	},
	"stbsprintf": {
		"path": "stb_sprintf-1.06",
		"includeDir": False,
		"compiled": False
	}
}

PATHS = {
	"win32-vcvarsall": WIN32_VCVARSALL
}