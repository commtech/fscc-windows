set TOP=lib

echo off
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
cd %~dp0

:building_driver
echo Building DLLs...
pushd %TOP%\c\ & nmake & popd > nul
pushd "%TOP%\c++\" & nmake & popd > nul
pushd %TOP%\net\ & nmake & popd > nul

exit