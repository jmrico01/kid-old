import sys
import os
import shutil
import platform
import string
import hashlib
import random

PROJECT_NAME = "kid"

def GetScriptPath():
    path = os.path.realpath(__file__)
    lastSep = string.rfind(path, os.sep)
    return path[:lastSep]

def GetLastSlashPath(path):
    """
    Returns the path before the last separating slash in path.
    (The path of the directory containing the given path.
    This is the equivalent of "path/..", but without the "..")
    """
    lastSep = string.rfind(path, os.sep)
    return path[:lastSep]

def NormalizePathSlashes(pathDict):
    for name in pathDict:
        pathDict[name] = pathDict[name].replace("/", os.sep)

# Important directory & file paths
paths = { "root": GetLastSlashPath(GetScriptPath()) }

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

# External dependencies
# TODO think of a better way of doing this
paths["win32-libs"] = "D:/Development/Libraries"
paths["include-freetype-win"] = paths["win32-libs"] + "/freetype-2.8.1/include"
paths["lib-freetype-win"] = paths["win32-libs"] + "/freetype-2.8.1/objs/vc2010/x64"

paths["include-libpng-win"] = paths["win32-libs"] + "/lpng1634"
paths["lib-libpng-win-d"] = paths["win32-libs"] + "/lpng1634/projects/vstudio/x64/DebugLibrary"
paths["lib-libpng-win-r"] = paths["win32-libs"] + "/lpng1634/projects/vstudio/x64/ReleaseLibrary"

paths["macos-libs"] = "/Users/legionjr/Documents/dev-libs/build"
paths["include-freetype-mac"] = paths["macos-libs"] + "/include/freetype2"
paths["lib-freetype-mac"] = paths["macos-libs"] + "/lib"

paths["include-libpng-mac"] = paths["macos-libs"] + "/include/libpng16"
paths["lib-libpng-mac"] = paths["macos-libs"] + "/lib"

paths["include-freetype-linux"] = "/usr/local/include/freetype2"
paths["lib-freetype-linux"] = "/usr/local/lib"

paths["include-libpng-linux"] = "/usr/local/include/libpng16"
paths["lib-libpng-linux"] = "/usr/local/lib"

NormalizePathSlashes(paths)

def WinCompileDebug():
    macros = " ".join([
        "/DGAME_INTERNAL=1",
        "/DGAME_SLOW=1",
        "/DGAME_WIN32",
        "/D_CRT_SECURE_NO_WARNINGS"
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
        "/LIBPATH:" + paths["lib-freetype-win"],
        "/LIBPATH:" + paths["lib-libpng-win-d"]
    ])
    libsGame = " ".join([
        "freetype281MTd.lib",
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

    loadCompiler = "call \"C:\\Program Files (x86)" + \
        "\\Microsoft Visual Studio 14.0\\VC\\vcvarsall.bat\" x64"
    os.system(" & ".join([
        "pushd " + paths["build"],
        loadCompiler,
        compileDLLCommand,
        compileCommand,
        devenvCommand,
        "popd"
    ]))

def WinCompileRelease():
    macros = " ".join([
        "/DGAME_INTERNAL=1",
        "/DGAME_SLOW=0",
        "/DGAME_WIN32",
        "/D_CRT_SECURE_NO_WARNINGS"
    ])
    compilerFlags = " ".join([
        "/MT",     # CRT static link (debug)
        "/nologo",  # disable the "Microsoft C/C++ Optimizing Compiler" message
        "/Gm-",     # disable incremental build things
        "/GR-",     # disable type information
        "/EHa-",    # disable exception handling
        "/EHsc",    # handle stdlib errors
        "/Ox",      # full optimization
        "/Z7"       # minimal "old school" debug information
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
        "/LIBPATH:" + paths["lib-freetype-win"],
        "/LIBPATH:" + paths["lib-libpng-win-r"]
    ])
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

    loadCompiler = "call \"C:\\Program Files (x86)" + \
        "\\Microsoft Visual Studio 14.0\\VC\\vcvarsall.bat\" x64"
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

def LinuxCompileDebug():
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

def MacCompileDebug():
    macros = " ".join([
        "-DGAME_INTERNAL=1",
        "-DGAME_SLOW=1",
        "-DGAME_MACOS"
    ])
    compilerFlags = " ".join([
        "-std=c++11",       # use C++11 standard
        "-lstdc++",         # link to C++ standard library
        "-g",
        #"-ggdb3",           # generate level 3 (max) GDB debug info.
        "-fno-rtti",        # disable run-time type info
        "-fno-exceptions"   # disable C++ exceptions (ew)
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

def MacCompileRelease():
    macros = " ".join([
        "-DGAME_INTERNAL=1",
        "-DGAME_SLOW=0",
        "-DGAME_MACOS"
    ])
    compilerFlags = " ".join([
        "-std=c++11",       # use C++11 standard
        "-lstdc++",         # link to C++ standard library
        "-O3",              # full optimiation
        "-fno-rtti",        # disable run-time type info
        "-fno-exceptions"   # disable C++ exceptions (ew)
    ])
    compilerWarningFlags = " ".join([
        "-Werror",  # treat warnings as errors
        "-Wall",    # enable all warnings

        # disable the following warnings:
        "-Wno-missing-braces",  # braces around initialization of subobject (?)
        "-Wno-char-subscripts", # using char as an array subscript
        "-Wno-unused-variable"  # unused variables when macros are removed
    ])
    includePaths = " ".join([
        "-I" + paths["include-freetype-mac"],
        "-I" + paths["include-libpng-mac"]
    ])

    frameworks = " ".join([
        "-framework Cocoa",
        "-framework OpenGL",
        "-framework AudioToolbox"
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

def Debug():
    ComputeSrcHashes()
    CopyDir(paths["data"], paths["build-data"])
    CopyDir(paths["src-shaders"], paths["build-shaders"])

    platformName = platform.system()
    if platformName == "Windows":
        WinCompileDebug()
    elif platformName == "Linux":
        LinuxCompileDebug()
    elif platformName == "Darwin":
        MacCompileDebug()
    else:
        print("Unsupported platform: " + platformName)
        

def IfChanged():
    hashPath = paths["src-hashes"]
    oldHashPath = paths["src-hashes-old"]

    changed = False
    if os.path.exists(hashPath):
        if os.path.exists(oldHashPath):
            os.remove(oldHashPath)
        os.rename(hashPath, oldHashPath)
    else:
        changed = True

    ComputeSrcHashes()
    if not changed:
        if os.path.getsize(hashPath) != os.path.getsize(oldHashPath) \
        or open(hashPath, "r").read() != open(oldHashPath, "r").read():
            changed = True
    
    if changed:
        Debug()
    else:
        print("No changes. Nothing to compile.")

def Release():
    CopyDir(paths["data"], paths["build-data"])
    CopyDir(paths["src-shaders"], paths["build-shaders"])

    platformName = platform.system()
    if platformName == "Windows":
        WinCompileRelease()
    elif platformName == "Linux":
        print("Release: UNIMPLEMENTED")
    elif platformName == "Darwin":
        MacCompileRelease()
    else:
        print("Release: UNIMPLEMENTED")

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
    if not os.path.exists(paths["build"]):
        os.makedirs(paths["build"])

    if (len(sys.argv) <= 1):
        print("Compile script expected at least one argument")
        return

    arg1 = sys.argv[1]
    if arg1 == "debug":
        Debug()
    elif arg1 == "ifchanged":
        IfChanged()
    elif arg1 == "release":
        Release()
    elif arg1 == "clean":
        Clean()
    elif arg1 == "run":
        Run()
    else:
        print("Unrecognized argument: " + arg1)

Main()
