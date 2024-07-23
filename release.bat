set NAME=fscc-windows-3.0.2.0
set TOP=release\%NAME%
set CONFIGURATION=\64
set SERIAL_DIR64=..\serialfc-windows\release%CONFIGURATION%
set PYFSCC=..\pyfscc\dist
set QFSCC=..\qfscc\build\exe.win32-3.3
set WFSCC=..\wfscc\build\exe.win32-3.3
set QSERIALFC=..\qserialfc\build\exe.win32-3.3

echo off

:reset_bin_folder
echo Removing old drivers...
rmdir /S /Q release\ > nul
rmdir /S /Q tmp\ > nul
mkdir %TOP%\ > nul
mkdir tmp\ > nul

:create_directories
echo Creating Directories...
for %%A in (64, lib, terminal, gui, examples, docs) do mkdir %TOP%\%%A\ > nul
for %%A in (fscc, serialfc) do mkdir %TOP%\lib\%%A\raw\ > nul
REM for %%A in (c, c++, net, python, raw) do mkdir %TOP%\lib\fscc\%%A\ > nul

REM this may not be necessary
REM :copy_serialfc
REM echo Copying serialfc files...
REM copy %SERIAL_DIR64%

:copy_dll_files
echo Copying DLL files...
copy lib\raw\*.h %TOP%\lib\fscc\raw\ > nul
copy lib\raw\*.c %TOP%\lib\fscc\raw\ > nul
REM serialfc files?

:copy_example_files
echo Copying Example Files...
xcopy examples\*.c %TOP%\examples\fscc\ /e /i > nul

:copy_docs_files
echo Copying Docs Files...
xcopy docs\*.md %TOP%\docs\fscc\ /e /i > nul

:copy_changelog
echo Copying Changelog...
copy ChangeLog.md %TOP% > nul

:copy_readme
echo Copying README...
copy README.md %TOP% > nul

:copy_knownissues
echo Copying KnownIssues
copy KnownIssues.md %TOP% > nul

:generate_cab
echo Generating .cab files for Windows 10..
makecab /f fscc_64.ddf > nul

:sign_cab
signtool sign /fd SHA512 /n "Commtech, Inc." /t http://timestamp.digicert.com/ /sha1 B757F8701A8CE35E8A243A12207CE8A697756DF0 %TOP%\64\fscc.cab
if %errorlevel% neq 0 exit /b %errorlevel%

:zip_packages
REM echo Zipping Drivers...
REM cd %TOP%\ > nul
REM cd ..\ > nul
REM ..\7za.exe a -tzip %NAME%.zip %NAME%\ > nul
REM cd ..\ > nul
