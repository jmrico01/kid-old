# Standard build script for Kapricorn Media projects
# Must be run from the root directory

import argparse
from enum import Enum
import hashlib
import json
import os
import platform
import random
import shutil
import sys

from app_info import PROJECT_NAME, DEPLOY_FILES, LIBS_EXTERNAL, PATHS

class CompileMode(Enum):
	DEBUG    = "debug"
	INTERNAL = "internal"
	RELEASE  = "release"

def NormalizePathSlashes(pathDict):
	for name in pathDict:
		pathDict[name] = pathDict[name].replace("/", os.sep)

# Important directory & file paths
paths = {}

paths["root"] = os.getcwd()

paths["build"]          = paths["root"]  + "/build"
paths["data"]           = paths["root"]  + "/data"
paths["deploy"]         = paths["root"]  + "/deploy"
paths["libs-external"]  = paths["root"]  + "/libs/external"
paths["libs-internal"]  = paths["root"]  + "/libs/internal"
paths["src"]            = paths["root"]  + "/src"

paths["build-data"]     = paths["build"] + "/data"
paths["build-logs"]     = paths["build"] + "/logs"
paths["build-shaders"]  = paths["build"] + "/shaders"
paths["src-shaders"]    = paths["src"]   + "/shaders"

# Main source file
paths["main-cpp"]       = paths["src"]   + "/main.cpp"

# Source hashes for if-changed compilation
paths["src-hashes"]     = paths["build"] + "/src_hashes"
paths["src-hashes-old"] = paths["build"] + "/src_hashes_old"

# External dependencies
"""
for name, libInfo in LIBS_EXTERNAL:
	paths["lib-" + name] = paths["libs-external"] + "/" + libInfo["path"]
	pathInclude = ""
	if "pathInclude" in libInfo:
		pathInclude = "/" + libInfo["pathInclude"]
	paths["include-" + name] = paths["lib-" + name] + pathInclude

if platform.system() == "Windows":
	paths["libdir-freetype-win32-d"] = paths["lib-freetype"] + "/win32/debug"
	paths["libdir-freetype-win32-r"] = paths["lib-freetype"] + "/win32/release"
"""

# Other project-specific paths
for name, path in PATHS.items():
	paths[name] = path

NormalizePathSlashes(paths)

includeDirs = {}
libCompiledBaseDirs = {}
libCompiledNames = {
	"debug": [],
	"release": []
}
for name, libInfo in LIBS_EXTERNAL.items():
	libPath = paths["libs-external"] + "/" + libInfo["path"]
	includeDirs[name] = libPath
	if libInfo["includeDir"]:
		includeDirs[name] += "/include"
	if libInfo["compiled"]:
		libCompiledBaseDirs[name] = libPath
		libCompiledNames["debug"].append(libInfo["compiledName"]["debug"])
		libCompiledNames["release"].append(libInfo["compiledName"]["release"])

NormalizePathSlashes(includeDirs)
NormalizePathSlashes(libCompiledBaseDirs)

def RemakeDestAndCopyDir(srcPath, dstPath):
	# Re-create (clear) the directory
	if os.path.exists(dstPath):
		shutil.rmtree(dstPath)
	os.makedirs(dstPath)

	# Copy
	for fileName in os.listdir(srcPath):
		filePath = os.path.join(srcPath, fileName)
		if os.path.isfile(filePath):
			shutil.copy2(filePath, dstPath)
		elif os.path.isdir(filePath):
			shutil.copytree(filePath, os.path.join(dstPath, fileName))

def ClearDirContents(path):
	for fileName in os.listdir(path):
		filePath = os.path.join(path, fileName)
		if os.path.isfile(filePath):
			os.remove(filePath)
		elif os.path.isdir(filePath):
			shutil.rmtree(filePath)

def WinCompile(compileMode, debugger):
	macros = " ".join([
		"/DGAME_WIN32=1",
		"/D_CRT_SECURE_NO_WARNINGS"
	])
	if compileMode == CompileMode.DEBUG:
		macros = " ".join([
			macros,
			"/DGAME_INTERNAL=1",
			"/DGAME_SLOW=1"
		])
	elif compileMode == CompileMode.INTERNAL:
		macros = " ".join([
			macros,
			"/DGAME_INTERNAL=1",
			"/DGAME_SLOW=0"
		])
	elif compileMode == CompileMode.RELEASE:
		macros = " ".join([
			macros,
			"/DGAME_INTERNAL=0",
			"/DGAME_SLOW=0"
		])

	compilerFlags = " ".join([
		"/nologo",  # disable the "Microsoft C/C++ Optimizing Compiler" message
		"/Gm-",     # disable incremental build things
		"/GR-",     # disable type information
		"/EHa-",    # disable exception handling
		"/EHsc"     # handle stdlib errors
	])
	if compileMode == CompileMode.DEBUG:
		compilerFlags = " ".join([
			compilerFlags,
			"/MTd", # CRT static link (debug)
			"/Od",  # no optimization
			"/Oi",  # ...except, optimize compiler intrinsics (do I need this?)
			"/Z7"   # minimal "old school" debug information
		])
	elif compileMode == CompileMode.INTERNAL or compileMode == CompileMode.RELEASE:
		compilerFlags = " ".join([
			compilerFlags,
			"/MT", # CRT static link
			"/Ox", # full optimization
			"/Z7"  # minimal "old school" debug information
		])

	compilerWarningFlags = " ".join([
		"/WX",      # treat warnings as errors
		"/W4",      # level 4 warnings

		# disable the following warnings:
		"/wd4100",  # unused function arguments
		"/wd4201",  # nonstandard extension used: nameless struct/union
		"/wd4505",  # unreferenced local function has been removed
	])

	includePaths = " ".join([
		"/I" + paths["src"],
		"/I" + paths["libs-internal"]
	] + [ "/I" + path for path in includeDirs.values() ])

	linkerFlags = " ".join([
		"/incremental:no",  # disable incremental linking
		"/opt:ref"          # get rid of extraneous linkages
	])

	libPaths = ""
	if compileMode == CompileMode.DEBUG:
		libPath = "/win32/debug"
	elif compileMode == CompileMode.INTERNAL or compileMode == CompileMode.RELEASE:
		libPath = "/win32/release"
	else:
		raise Exception("Unknown compile mode {}".format(compileMode))
	libPaths = " ".join([
		libPaths
	] + [ "/LIBPATH:" + baseDir + libPath for baseDir in libCompiledBaseDirs.values() ])

	libs = " ".join([
		"user32.lib",
		"gdi32.lib",
		"opengl32.lib",
		"ole32.lib",
		"winmm.lib"
	])
	if compileMode == CompileMode.DEBUG:
		libs = " ".join([
			libs
		] + [ libName for libName in libCompiledNames["debug"] ])
	elif compileMode == CompileMode.INTERNAL or compileMode == CompileMode.RELEASE:
		libs = " ".join([
			libs
		] + [ libName for libName in libCompiledNames["release"] ])
	else:
		raise Exception("Unknown compile mode {}".format(compileMode))

	# Clear old PDB files
	for fileName in os.listdir(paths["build"]):
		if ".pdb" in fileName:
			try:
				os.remove(os.path.join(paths["build"], fileName))
			except:
				print("Couldn't remove " + fileName)

	pdbName = PROJECT_NAME + "_game" + str(random.randrange(99999)) + ".pdb"

	exeFileName = PROJECT_NAME + "_win32.exe"
	compileCommand = " ".join([
		"cl",
		macros, compilerFlags, compilerWarningFlags, includePaths,
		"/Fe" + exeFileName,
		"/Fm" + PROJECT_NAME + "_win32.map",
		paths["main-cpp"],
		"/link", linkerFlags, libPaths, libs,
		"/PDB:" + pdbName
	])
	
	devenvCommand = "rem"
	if debugger:
		devenvCommand = "devenv " + exeFileName

	loadCompiler = "call \"" + paths["win32-vcvarsall"] + "\" x64"
	os.system(" & ".join([
		"pushd " + paths["build"],
		loadCompiler,
		# compileDLLCommand,
		compileCommand,
		devenvCommand,
		"popd"
	]))

def WinRun():
	os.system(" & ".join([
		"pushd " + paths["build"],
		PROJECT_NAME + "_win32.exe",
		"popd"
	]))

def WinDeploy():
	if not os.path.exists(paths["deploy"]):
		os.makedirs(paths["deploy"])

	deployBundleName = PROJECT_NAME
	deployBundlePath = os.path.join(paths["deploy"], deployBundleName)
	RemakeDestAndCopyDir(paths["build"], deployBundlePath)
	for fileName in os.listdir(deployBundlePath):
		if fileName not in DEPLOY_FILES:
			filePath = os.path.join(deployBundlePath, fileName)
			if os.path.isfile(filePath):
				os.remove(filePath)
			elif os.path.isdir(filePath):
				shutil.rmtree(filePath)

	deployZipPath = os.path.join(paths["deploy"], "0. Unnamed")
	shutil.make_archive(deployZipPath, "zip", root_dir=paths["deploy"], base_dir=deployBundleName)

def LinuxCompile(compileMode):
	macros = " ".join([
		"-DGAME_INTERNAL=1",
		"-DGAME_SLOW=1",
		"-DGAME_LINUX"
	])
	compilerFlags = " ".join([
		"-std=c++11",       # use C++11 standard
		"-ggdb3",           # generate level 3 (max) GDB debug info.
		"-fno-rtti",        # disable run-time type info
		"-fno-exceptions"   # disable C++ exceptions (ew)
	])
	compilerWarningFlags = " ".join([
		"-Werror",  # treat warnings as errors
		"-Wall",    # enable all warnings

		# disable the following warnings:
		"-Wno-char-subscripts" # using char as an array subscript
	])
	includePaths = " ".join([
		"-I" + paths["include-freetype-linux"]
	])

	linkerFlags = " ".join([
		"-fvisibility=hidden"
	])
	libPathsPlatform = " ".join([
	])
	libsPlatform = " ".join([
		"-lm",      # math
		"-ldl",     # dynamic linking loader
		"-lGL",     # OpenGL
		"-lX11",    # X11
		"-lasound", # ALSA lib
		"-lpthread"
	])
	libPathsGame = " ".join([
		"-L" + paths["lib-freetype-linux"]
	])
	libsGame = " ".join([
		"-lfreetype"
	])

	#pdbName = PROJECT_NAME + "_game" + str(random.randrange(99999)) + ".pdb"
	compileLibCommand = " ".join([
		"gcc",
		macros, compilerFlags, compilerWarningFlags, includePaths,
		"-shared", "-fPIC", paths["main-cpp"],
		"-o " + PROJECT_NAME + "_game.so",
		linkerFlags, "-static", libPathsGame, libsGame
	])

	compileCommand = " ".join([
		"gcc", "-DGAME_PLATFORM_CODE",
		macros, compilerFlags, compilerWarningFlags, includePaths,
		paths["linux-main-cpp"],
		"-o " + PROJECT_NAME + "_linux",
		linkerFlags, libPathsPlatform, libsPlatform
	])

	os.system("bash -c \"" + " ; ".join([
		"pushd " + paths["build"] + " > /dev/null",
		compileLibCommand,
		compileCommand,
		"popd > /dev/null"
	]) + "\"")

def LinuxRun():
	os.system(paths["build"] + os.sep + PROJECT_NAME + "_linux")

def MacCompile(compileMode):
	macros = " ".join([
		"-DGAME_MACOS"
	])
	if compileMode == CompileMode.DEBUG:
		macros = " ".join([
			macros,
			"-DGAME_INTERNAL=1",
			"-DGAME_SLOW=1"
		])
	elif compileMode == CompileMode.INTERNAL:
		macros = " ".join([
			macros,
			"-DGAME_INTERNAL=1",
			"-DGAME_SLOW=0"
		])
	elif compileMode == CompileMode.RELEASE:
		macros = " ".join([
			macros,
			"-DGAME_INTERNAL=0",
			"-DGAME_SLOW=0"
		])

	compilerFlags = " ".join([
		"-std=c++11",     # use C++11 standard
		"-lstdc++",       # link to C++ standard library
		"-fno-rtti",      # disable run-time type info
		"-fno-exceptions" # disable C++ exceptions (ew)
	])
	if compileMode == CompileMode.DEBUG:
		compilerFlags = " ".join([
			compilerFlags,
			"-g" # generate debug info
		])
	elif compileMode == CompileMode.INTERNAL or compileMode == CompileMode.RELEASE:
		compilerFlags = " ".join([
			compilerFlags,
			"-O3" # full optimization
		])

	compilerWarningFlags = " ".join([
		"-Werror",  # treat warnings as errors
		"-Wall",    # enable all warnings

		# disable the following warnings:
		"-Wno-missing-braces",  # braces around initialization of subobject (?)
		"-Wno-char-subscripts", # using char as an array subscript
		"-Wno-unused-function"
	])

	includePaths = " ".join([
		"-I" + paths["include-freetype-mac"]
	])

	frameworks = " ".join([
		"-framework Cocoa",
		"-framework OpenGL",
		"-framework AudioToolbox",
		"-framework CoreMIDI"
	])
	linkerFlags = " ".join([
		#"-fvisibility=hidden"
	])
	libPaths = " ".join([
		"-L" + paths["lib-freetype-mac"]
	])
	libs = " ".join([
		"-lfreetype"
	])

	compileLibCommand = " ".join([
		"clang",
		macros, compilerFlags, compilerWarningFlags, includePaths,
		"-dynamiclib", paths["main-cpp"],
		"-o " + PROJECT_NAME + "_game.dylib",
		linkerFlags, libPaths, libs
	])

	compileCommand = " ".join([
		"clang", "-DGAME_PLATFORM_CODE",
		macros, compilerFlags, compilerWarningFlags, #includePaths,
		frameworks,
		paths["macos-main-mm"],
		"-o " + PROJECT_NAME + "_macos"
	])

	os.system("bash -c \"" + " ; ".join([
		"pushd " + paths["build"] + " > /dev/null",
		compileLibCommand,
		compileCommand,
		"popd > /dev/null"
	]) + "\"")

def MacRun():
	os.system(paths["build"] + os.sep + PROJECT_NAME + "_macos")

def FileMD5(filePath):
	md5 = hashlib.md5()
	with open(filePath, "rb") as f:
		for chunk in iter(lambda: f.read(4096), b""):
			md5.update(chunk)
	
	return md5.hexdigest()

def ComputeSrcHashes():
	with open(paths["src-hashes"], "w") as out:
		for root, _, files in os.walk(paths["src"]):
			for fileName in files:
				filePath = os.path.join(root, fileName)
				out.write(filePath + "\n")
				out.write(FileMD5(filePath) + "\n")

def DidFilesChange():
	hashPath = paths["src-hashes"]
	oldHashPath = paths["src-hashes-old"]

	if os.path.exists(hashPath):
		if os.path.exists(oldHashPath):
			os.remove(oldHashPath)
		os.rename(hashPath, oldHashPath)
	else:
		return True

	ComputeSrcHashes()
	if os.path.getsize(hashPath) != os.path.getsize(oldHashPath) \
	or open(hashPath, "r").read() != open(oldHashPath, "r").read():
		return True

	return False

def Clean():
	ClearDirContents(paths["build"])
	ClearDirContents(paths["deploy"])

def Run():
	platformName = platform.system()
	if platformName == "Windows":
		WinRun()
	elif platformName == "Linux":
		LinuxRun()
	elif platformName == "Darwin":
		MacRun()
	else:
		print("Unsupported platform: " + platformName)

def Main():
	parser = argparse.ArgumentParser()
	parser.add_argument("mode", help="compilation mode")
	parser.add_argument("--ifchanged", action="store_true",
		help="run the specified compile command only if files have changed")
	parser.add_argument("--debugger", action="store_true",
		help="open the platform debugger after compiling")
	parser.add_argument("--deploy", action="store_true",
		help="package and deploy a game build after compiling")
	args = parser.parse_args()

	if not os.path.exists(paths["build"]):
		os.makedirs(paths["build"])

	if args.ifchanged:
		if not DidFilesChange():
			print("No changes, nothing to compile")
			return

	compileModeDict = { cm.value: cm for cm in list(CompileMode) }

	if args.mode == "clean":
		Clean()
	elif args.mode == "run":
		Run()
	elif args.mode in compileModeDict:
		ComputeSrcHashes()
		RemakeDestAndCopyDir(paths["data"], paths["build-data"])
		RemakeDestAndCopyDir(paths["src-shaders"], paths["build-shaders"])
		if not os.path.exists(paths["build-logs"]):
			os.makedirs(paths["build-logs"])

		compileMode = compileModeDict[args.mode]
		platformName = platform.system()
		if platformName == "Windows":
			WinCompile(compileMode, args.debugger)
			if args.deploy:
				WinDeploy()
		elif platformName == "Linux":
			LinuxCompile(compileMode)
		elif platformName == "Darwin":
			MacCompile(compileMode)
		else:
			print("Unsupported platform: " + platformName)
	else:
		print("Unrecognized argument: " + args.mode)

if __name__ == "__main__":
	Main()
