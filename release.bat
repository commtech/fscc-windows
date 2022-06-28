set NAME=fscc-windows-2.8.1
set TOP=bin\%NAME%
set PYFSCC=..\pyfscc\dist
set QFSCC=..\qfscc\build\exe.win32-3.3
set WFSCC=..\wfscc\build\exe.win32-3.3
set QSERIALFC=..\qserialfc\build\exe.win32-3.3

echo off

:reset_bin_folder
echo Removing Old Drivers...
rmdir /S /Q bin\ 2> nul
mkdir %TOP%\ > nul
mkdir bin\10 > nul
mkdir bin\10\32 > nul
mkdir bin\10\64 > nul
rmdir /S /Q tmp\production\ 2> nul
mkdir tmp\production\ > nul

:build_drivers
echo Building Release Drivers...
start /I /WAIT build_driver.bat "fre x86 WXP" objfre_wxp_x86 i386 production XP_X86
if %errorlevel% neq 0 exit /b %errorlevel%
start /I /WAIT build_driver.bat "fre x64 WNET" objfre_wnet_amd64 amd64 production Server2003_X64
if %errorlevel% neq 0 exit /b %errorlevel%

:generate_cab
echo Generating .cab files for Windows 10..
makecab /f fscc_32.ddf
makecab /f fscc_64.ddf

:signtool sign /fd SHA512 /ac "DigiCert High Assurance EV Root CA.crt" /n "Commtech, Inc." /t http://timestamp.digicert.com/ /sha1 A55F5C61CE305CD8F45E6C46AD704C452FBF630E bin\10\64\fscc.cab
:signtool sign /fd SHA512 /t http://timestamp.digicert.com/ /sha1 A55F5C61CE305CD8F45E6C46AD704C452FBF630E bin\10\64\fscc.cab
:if %errorlevel% neq 0 exit /b %errorlevel%
:signtool sign /fd SHA512 /ac "DigiCert High Assurance EV Root CA.crt" /n "Commtech, Inc." /t http://timestamp.digicert.com/ /sha1 A55F5C61CE305CD8F45E6C46AD704C452FBF630E bin\10\32\fscc.cab
:signtool sign /fd SHA512 /t http://timestamp.digicert.com/ /sha1 A55F5C61CE305CD8F45E6C46AD704C452FBF630E bin\10\32\fscc.cab
:if %errorlevel% neq 0 exit /b %errorlevel%

:build_libs
echo Building Libraries...
start /I /WAIT build_libs.bat
if %errorlevel% neq 0 exit /b %errorlevel%

:create_directories
echo Creating Directories...
for %%A in (32, 64, lib, terminal, gui, tools) do mkdir %TOP%\%%A\
for %%A in (fscc, serialfc) do mkdir %TOP%\lib\%%A\
for %%A in (c, c++, net, python, raw) do mkdir %TOP%\lib\fscc\%%A\

:copy_dll_files
echo Copying DLL Files...
copy lib\raw\*.h %TOP%\lib\fscc\raw\ > nul
copy lib\raw\*.c %TOP%\lib\fscc\raw\ > nul

mkdir %TOP%\lib\fscc\c\src\
mkdir %TOP%\lib\fscc\c\docs\
mkdir %TOP%\lib\fscc\c\examples\
copy lib\c\cfscc*.dll %TOP%\lib\fscc\c\ > nul
copy lib\c\cfscc*.lib %TOP%\lib\fscc\c\ > nul
copy lib\c\src\*.c %TOP%\lib\fscc\c\src\ > nul
copy lib\c\src\*.h %TOP%\lib\fscc\c\src\ > nul
copy lib\c\docs\*.md %TOP%\lib\fscc\c\docs\ > nul
copy lib\c\examples\*.c %TOP%\lib\fscc\c\examples\ > nul

mkdir "%TOP%\lib\fscc\c++\src\"
mkdir "%TOP%\lib\fscc\c++\docs\"
mkdir "%TOP%\lib\fscc\c++\examples\"
copy "lib\c++\cppfscc*.dll" "%TOP%\lib\fscc\c++\" > nul
copy "lib\c++\cppfscc*.lib" "%TOP%\lib\fscc\c++\" > nul
copy "lib\c++\src\*.cpp" "%TOP%\lib\fscc\c++\src\" > nul
copy "lib\c++\src\*.hpp" "%TOP%\lib\fscc\c++\src\" > nul
copy "lib\c++\docs\*.md" "%TOP%\lib\fscc\c++\docs\" > nul
copy "lib\c++\examples\*.cpp" "%TOP%\lib\fscc\c++\examples\" > nul
copy lib\c\cfscc*.dll "%TOP%\lib\fscc\c++\" > nul

mkdir %TOP%\lib\fscc\net\src\
mkdir %TOP%\lib\fscc\net\docs\
mkdir %TOP%\lib\fscc\net\examples\
copy lib\net\netfscc*.dll %TOP%\lib\fscc\net\ > nul
copy lib\net\src\*.cs %TOP%\lib\fscc\net\src\ > nul
copy lib\net\docs\*.md %TOP%\lib\fscc\net\docs\ > nul
copy lib\net\examples\*.cs %TOP%\lib\fscc\net\examples\ > nul
copy lib\c\cfscc*.dll %TOP%\lib\fscc\net\ > nul

xcopy redist\production\serial\lib\* %TOP%\lib\serialfc\ /e /i > nul
xcopy redist\production\serial\terminal\* %TOP%\terminal\serialfc\ /e /i > nul
xcopy redist\production\serial\gui\* %TOP%\gui\serialfc\ /e /i > nul

xcopy %QFSCC%\* %TOP%\gui\fscc\ /e /i > nul
xcopy %QSERIAL%\* %TOP%\gui\serial\ /e /i > nul

:copy_loop_files
echo Copying Loop Files...
xcopy lib\c\utils\loop\loop.exe %TOP%\lib\fscc\c\loop\ /i > nul
xcopy lib\c\utils\loop\loop.c %TOP%\lib\fscc\c\loop\ /i > nul
xcopy lib\c\utils\loop\makefile %TOP%\lib\fscc\c\loop\ /i > nul
xcopy lib\c\cfscc.dll %TOP%\lib\fscc\c\loop\ /i > nul

:copy_example_files
echo Copying Example Files...
xcopy examples\*.c %TOP%\examples\fscc\ /e /i > nul

:copy_tool_files
echo Copying Tool Files...
xcopy tools\*.c %TOP%\tools\fscc\ /e /i > nul

:copy_docs_files
echo Copying Docs Files...
xcopy docs\*.md %TOP%\docs\fscc\ /e /i > nul

:copy_test_files
:echo Copying Test Files...
:copy lib\c\utils\test\test.exe %TOP%\test\ > nul
:copy lib\c\cfscc.dll %TOP%\test\ > nul

:copy_wireshark_files
echo Copying Wireshark Files...
xcopy %WFSCC%\* %TOP%\wireshark\ /e /i > nul

:copy_sys_files
echo Copying Driver Files...
copy tmp\production\i386\* %TOP%\32\ > nul
copy tmp\production\amd64\* %TOP%\64\ > nul

:copy_python_files
echo Copying Python Files...
copy %PYFSCC%\pyfscc*.exe* %TOP%\lib\fscc\python\ > nul

:copy_setup_files
:echo Copying Setup Files...
:copy redist\production\i386\dpinst.exe %TOP%\32\setup.exe > nul
:copy redist\production\amd64\dpinst.exe %TOP%\64\setup.exe > nul

:copy_changelog
echo Copying Changelog...
copy ChangeLog.md %TOP% > nul

:copy_readme
echo Copying README...
copy README.md %TOP% > nul

:zip_packages
echo Zipping Drivers...
cd %TOP%\ > nul
cd ..\ > nul
..\7za.exe a -tzip %NAME%.zip %NAME%\ > nul
cd ..\ > nul
