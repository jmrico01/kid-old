from compile import Platform, TargetType, Define, PlatformTargetOptions, BuildTarget, CopyDir, LibExternal
from env_settings import WIN32_VCVARSALL

windows_options = PlatformTargetOptions(
	defines=[],
	compiler_flags=[
		"/wd4100", # unreferenced formal parameter
		"/wd4201", # nonstandard extension used: nameless struct/union
		"/wd4458", # declaration of X hides class member
		"/wd4505", # unreferenced local function has been removed
	],
	linker_flags=[]
)

TARGETS = [
    BuildTarget("kid",
        source_file="src/main.cpp",
        type=TargetType.EXECUTABLE,
		defines=[],
        platform_options={
			Platform.WINDOWS: windows_options
		}
    ),
    # BuildTarget("test_bench",
    #    source_file="src/test_bench.cpp",
    #    type=TargetType.EXECUTABLE,
	#	platform_options={
	#		Platform.WINDOWS: windows_options
	#	}
    #)
]

COPY_DIRS = [
    CopyDir("data", "data"),
    CopyDir("src/shaders", "shaders")
]

DEPLOY_FILES = [
    "data",
    "logs",
    "shaders",
    TARGETS[0].get_output_name()
]

LIBS_EXTERNAL = [
    LibExternal("freetype",
        path="freetype-2.8.1",
        compiledNames={
            "debug":   "freetype281MTd.lib",
            "release": "freetype281MT.lib"
        }
    ),
    LibExternal("stbimage",   path="stb_image-2.23"),
    LibExternal("stbsprintf", path="stb_sprintf-1.06"),
    LibExternal("utf8proc",   path="utf8proc")
]

PATHS = {
    "win32-vcvarsall": WIN32_VCVARSALL
}

def post_compile_custom(paths):
    pass
