set TOP=lib

echo off

:building_driver
echo Building DLLs...
pushd %TOP%\c\ & nmake & popd > nul
pushd "%TOP%\c++\" & nmake & popd > nul
pushd %TOP%\net\ & nmake & popd > nul

exit