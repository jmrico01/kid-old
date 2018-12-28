@echo off

pushd ..\src

set Wildcard=*.h *.cpp *.c

echo -----------------------------------------------------------
echo STATICS FOUND:
FINDSTR /S /N /L "static" %Wildcard%

echo -----------------------------------------------------------
echo -----------------------------------------------------------

echo GLOBALS FOUND:
findstr /S /N /L "local_persist" %Wildcard%
findstr /S /N /L "global_var" %Wildcard%
echo -----------------------------------------------------------

popd