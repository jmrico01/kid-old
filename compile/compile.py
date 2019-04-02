import argparse
from enum import Enum
import hashlib
import json
import os
import platform
import random
import shutil
import sys
import string

PROJECT_NAME = "kid"

class CompileMode(Enum):
    DEBUG    = "debug"
    INTERNAL = "internal"
    RELEASE  = "release"

def GetScriptPath():
    path = os.path.realpath(__file__)
    lastSep = path.rfind(os.sep)
    return path[:lastSep]

def GetEnclosingDir(path):
    """
    Returns the path before the last separating slash in path.
    (The path of the directory containing the given path.
    This is the equivalent of "path/..", but without the "..")
    """
    lastSep = path.rfind(os.sep)
    return path[:lastSep]

def NormalizePathSlashes(pathDict):
    for name in pathDict:
        pathDict[name] = pathDict[name].replace("/", os.sep)

def LoadEnvSettings(pathDict, envSettingsPath):
    """
    You must create a file called "env_settings.json" in the same directory as this script.
    It must contain at least a "osName": { "libs": "<path-to-external-libs>" } entry
    """
    with open(envSettingsPath, "r") as envSettingsFile:
        envSettings = json.loads(envSettingsFile.read())

    for envOS in envSettings:
        for envSetting in envSettings[envOS]:
            pathDict[envOS + "-" + envSetting] = envSettings[envOS][envSetting]

# Important directory & file paths
paths = { "root": GetEnclosingDir(GetScriptPath()) }

paths["build"]          = paths["root"] + "/build"
paths["data"]           = paths["root"] + "/data"
paths["src"]            = paths["root"] + "/src"

paths["build-data"]     = paths["build"] + "/data"
paths["build-shaders"]  = paths["build"] + "/shaders"
paths["src-shaders"]    = paths["src"] + "/shaders"

# Main source files
paths["main-cpp"]       = paths["src"] + "/main.cpp"
paths["linux-main-cpp"] = paths["src"] + "/linux_main.cpp"
paths["macos-main-mm"]  = paths["src"] + "/macos_main.mm"
paths["win32-main-cpp"] = paths["src"] + "/win32_main.cpp"

# Source hashes for if-changed compilation
paths["src-hashes"]     = paths["build"] + "/src_hashes"
paths["src-hashes-old"] = paths["build"] + "/src_hashes_old"

paths["env-settings"]   = paths["root"] + "/compile/env_settings.json"

NormalizePathSlashes(paths)
LoadEnvSettings(paths, paths["env-settings"])
NormalizePathSlashes(paths)

# External dependencies
if platform.system() == "Windows":
    paths["include-freetype-win"] = paths["win32-libs"] + "/freetype-2.8.1/include"
    paths["lib-freetype-win-d"] = paths["win32-libs"] + "/freetype-2.8.1/lib-d"
    paths["lib-freetype-win-r"] = paths["win32-libs"] + "/freetype-2.8.1/lib-r"

    paths["include-libpng-win"] = paths["win32-libs"] + "/lpng1634/include"
    paths["lib-libpng-win-d"] = paths["win32-libs"] + "/lpng1634/lib-d"
    paths["lib-libpng-win-r"] = paths["win32-libs"] + "/lpng1634/lib-r"

if platform.system() == "Darwin":
    paths["include-freetype-mac"] = paths["macos-libs"] + "/include/freetype2"
    paths["lib-freetype-mac"] = paths["macos-libs"] + "/lib"

    paths["include-libpng-mac"] = paths["macos-libs"] + "/include/libpng16"
    paths["lib-libpng-mac"] = paths["macos-libs"] + "/lib"

if platform.system() == "Linux":
    paths["include-freetype-linux"] = "/usr/local/include/freetype2"
    paths["lib-freetype-linux"] = "/usr/local/lib"

    paths["include-libpng-linux"] = "/usr/local/include/libpng16"
    paths["lib-libpng-linux"] = "/usr/local/lib"

NormalizePathSlashes(paths)

def WinCompile(compileMode):
    macros = " ".join([
        "/DGAME_WIN32",
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
        "/MTd",     # CRT static link (debug)
        "/nologo",  # disable the "Microsoft C/C++ Optimizing Compiler" message
        "/Gm-",     # disable incremental build things
        "/GR-",     # disable type information
        "/EHa-",    # disable exception handling
        "/EHsc",    # handle stdlib errors
        "/Od",      # no optimization
        "/Oi",      # ...except, optimize compiler intrinsics (do I need this?)
        "/Z7"       # minimal "old school" debug information
    ])
    if compileMode == CompileMode.DEBUG:
        compilerFlags = " ".join([
            compilerFlags,
            "/MTd",
            "/Od",
            "/Oi",
            "/Z7"
        ])
    elif compileMode == CompileMode.INTERNAL or compileMode == CompileMode.RELEASE:
        compilerFlags = " ".join([
            compilerFlags,
            "/MT",
            "/Ox"
        ])
    compilerWarningFlags = " ".join([
        "/WX",      # treat warnings as errors
        "/W4",      # level 4 warnings

        # disable the following warnings:
        "/wd4100",  # unused function arguments
        "/wd4189",  # unused initialized local variable
        "/wd4201",  # nonstandard extension used: nameless struct/union
        "/wd4505",  # unreferenced local function has been removed
    ])
    includePaths = " ".join([
        "/I" + paths["include-freetype-win"],
        "/I" + paths["include-libpng-win"]
    ])

    linkerFlags = " ".join([
        "/incremental:no",  # disable incremental linking
        "/opt:ref"          # get rid of extraneous linkages
    ])
    libPathsPlatform = " ".join([
    ])
    libsPlatform = " ".join([
        "user32.lib",
        "gdi32.lib",
        "opengl32.lib",
        "ole32.lib",
        "winmm.lib"
    ])
    libPathsGame = " ".join([
        "/LIBPATH:" + paths["lib-freetype-win-d"],
        "/LIBPATH:" + paths["lib-libpng-win-d"]
    ])
    if compileMode == CompileMode.INTERNAL or compileMode == CompileMode.RELEASE:
        libPathsGame = " ".join([
            "/LIBPATH:" + paths["lib-freetype-win-r"],
            "/LIBPATH:" + paths["lib-libpng-win-r"]
        ])
    libsGame = " ".join([
        "freetype281MTd.lib",
        "libpng16.lib",
        "zlib.lib"
    ])
    if compileMode == CompileMode.INTERNAL or compileMode == CompileMode.RELEASE:
        libsGame = " ".join([
            "freetype281MT.lib",
            "libpng16.lib",
            "zlib.lib"
        ])

    # Clear old PDB files
    for fileName in os.listdir(paths["build"]):
        if ".pdb" in fileName:
            try:
                os.remove(os.path.join(paths["build"], fileName))
            except:
                print("Couldn't remove " + fileName)

    pdbName = PROJECT_NAME + "_game" + str(random.randrange(99999)) + ".pdb"
    compileDLLCommand = " ".join([
        "cl",
        macros, compilerFlags, compilerWarningFlags, includePaths,
        "/LD", "/Fe" + PROJECT_NAME + "_game.dll",
        paths["main-cpp"],
        "/link", linkerFlags, libPathsGame, libsGame,
        "/EXPORT:GameUpdateAndRender", "/PDB:" + pdbName])

    compileCommand = " ".join([
        "cl", "/DGAME_PLATFORM_CODE",
        macros, compilerFlags, compilerWarningFlags, includePaths,
        "/Fe" + PROJECT_NAME + "_win32.exe",
        "/Fm" + PROJECT_NAME + "_win32.map",
        paths["win32-main-cpp"],
        "/link", linkerFlags, libPathsPlatform, libsPlatform])
    
    devenvCommand = "rem"
    if len(sys.argv) > 2:
        if sys.argv[2] == "devenv":
            devenvCommand = "devenv " + PROJECT_NAME + "_win32.exe"

    loadCompiler = "call \"" + paths["win32-vcvarsall"] + "\" x64"
    os.system(" & ".join([
        "pushd " + paths["build"],
        loadCompiler,
        compileDLLCommand,
        compileCommand,
        devenvCommand,
        "popd"
    ]))

def WinRun():
    os.system(paths["build"] + os.sep + PROJECT_NAME + "_win32.exe")

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
        "-I" + paths["include-freetype-linux"],
        "-I" + paths["include-libpng-linux"]
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
        "-L" + paths["lib-freetype-linux"],
        "-L" + paths["lib-libpng-linux"]
    ])
    libsGame = " ".join([
        "-lfreetype",
        "-lpng",

        #"-lm",      # math
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
        "-I" + paths["include-freetype-mac"],
        "-I" + paths["include-libpng-mac"]
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
        "-L" + paths["lib-freetype-mac"],
        "-L" + paths["lib-libpng-mac"]
    ])
    libs = " ".join([
        "-lfreetype",
        "-lpng"
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

def CopyDir(srcPath, dstPath):
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
            shutil.copytree(filePath, dstPath + os.sep + fileName)

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
    for fileName in os.listdir(paths["build"]):
        filePath = os.path.join(paths["build"], fileName)
        try:
            if os.path.isfile(filePath):
                os.remove(filePath)
            elif os.path.isdir(filePath):
                shutil.rmtree(filePath)
        except Exception as e:
            # Handles file-in-use kinds of things.
            # ... exceptions are so ugly.
            print(e)

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
        CopyDir(paths["data"], paths["build-data"])
        CopyDir(paths["src-shaders"], paths["build-shaders"])

        compileMode = compileModeDict[args.mode]
        platformName = platform.system()
        if platformName == "Windows":
            WinCompile(compileMode)
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
