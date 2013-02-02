set NAME=fscc
set TOP=bin\%NAME%

echo off

:reset_bin_folder
echo Removing Old Drivers...
rmdir /S /Q bin\ 2> nul
mkdir %TOP%\
rmdir /S /Q tmp\production\ 2> nul
mkdir tmp\production\

:build_drivers
echo Building Release Drivers...
start /I /WAIT build_driver.bat "fre x86 WXP" objfre_wxp_x86 i386 production XP_X86
start /I /WAIT build_driver.bat "fre x64 WNET" objfre_wnet_amd64 amd64 production Server2003_X64

:build_libs
echo Building Libraries...
start /I /WAIT build_libs.bat

:build_docs
doxygen > nul

:create_directories
echo Creating Directories...
for %%A in (32, 64, lib) do mkdir %TOP%\%%A\
for %%A in (fscc, serialfc) do mkdir %TOP%\lib\%%A\
for %%A in (c, c++, net) do mkdir %TOP%\lib\fscc\%%A\

:copy_dll_files
echo Copying DLL Files...
copy lib\c\cfscc*.dll %TOP%\lib\fscc\c\ > nul
copy lib\c\cfscc*.lib %TOP%\lib\fscc\c\ > nul
copy lib\c\src\*.c %TOP%\lib\fscc\c\ > nul
copy lib\c\src\*.h %TOP%\lib\fscc\c\ > nul
copy lib\c\makefile %TOP%\lib\fscc\c\ > nul
copy "lib\c++\cppfscc*.dll" "%TOP%\lib\fscc\c++\" > nul
copy "lib\c++\cppfscc*.lib" "%TOP%\lib\fscc\c++\" > nul
copy lib\c\cfscc*.dll "%TOP%\lib\fscc\c++\" > nul
copy "lib\c++\src\*.cpp" "%TOP%\lib\fscc\c++\" > nul
copy "lib\c++\src\*.hpp" "%TOP%\lib\fscc\c++\" > nul
copy "lib\c++\makefile" "%TOP%\lib\fscc\c++\" > nul
copy lib\net\netfscc*.dll %TOP%\lib\fscc\net\ > nul
copy lib\c\cfscc*.dll %TOP%\lib\fscc\net\ > nul
copy lib\net\src\*.cs %TOP%\lib\fscc\net\ > nul
copy lib\net\makefile %TOP%\lib\fscc\net\ > nul
xcopy redist\production\serial\lib\* %TOP%\lib\serialfc\ /e /i > nul

:copy_sys_files
echo Copying Driver Files...
copy tmp\production\i386\* %TOP%\32\ > nul
copy tmp\production\amd64\* %TOP%\64\ > nul

:copy_setup_files
:echo Copying Setup Files...
:copy redist\production\i386\dpinst.exe %TOP%\32\setup.exe > nul
:copy redist\production\amd64\dpinst.exe %TOP%\64\setup.exe > nul

:copy_docs
echo Copying Docs...
xcopy lib\c\docs %TOP%\lib\fscc\c\docs /e /i > nul
xcopy lib\c\examples %TOP%\lib\fscc\c\examples /e /i > nul

:copy_changelog
echo Copying Changelog...
copy ChangeLog.txt %TOP% > nul

:zip_packages
echo Zipping drivers...
cd %TOP%\ > nul
cd ..\ > nul
..\7za.exe a -tzip %NAME%.zip %NAME%\ > nul
cd ..\ > nul
