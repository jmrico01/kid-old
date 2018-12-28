@echo off

pushd ..\src

set Wildcard=*.h *.cpp *.c

echo -----------------------------------------------------------
echo TODO:
FINDSTR /S /N /L "TODO" %Wildcard%
echo -----------------------------------------------------------

popd