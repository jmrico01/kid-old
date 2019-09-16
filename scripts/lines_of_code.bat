@echo off

SET cloc_path="C:\Users\RicoJose\Documents\dev-other\cloc\cloc-1.82.exe"
REM SET cloc_path="D:\Development\cloc\cloc-1.82.exe"

%cloc_path% ^
	--by-file ^
    --include-lang="C++","C/C++ Header","GLSL","Objective C++","Python","DOS Batch" ^
    .\src\* .\libs\internal\* .\scripts\* .\compile\*