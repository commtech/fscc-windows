set TOP=tmp\%4\%3
set NAME=fscc
set CODE=%~dp0

echo off
call C:\WinDDK\7600.16385.1\bin\setenv.bat C:\WinDDK\7600.16385.1\ %~1
cd %CODE%

:building_driver
echo Building Driver...
pushd src\
build -cfeg
popd

:reset_tmp_folder
echo Removing Old Drivers...
rmdir /S /Q %TOP%\ 2> nul
mkdir %TOP%\

:copy_sys_files
echo Copying Driver Files...
copy src\%2\%3\fscc.sys %TOP%\ > nul

:copy_inf_files
echo Copying Installation Files...
copy src\%2\%3\fscc.inf %TOP%\ > nul

:copy_wdf_files
echo Copying Redistribution Files...
copy redist\%4\%3\* %TOP%\ > nul

:create_catalogs
echo Creating Driver Catalogs...
Inf2cat.exe /driver:%TOP%\ /os:%5 > nul

:sign_files
echo Signing Files...
signtool sign /v /ac MSCV-VSClass3.cer /s my /n "Commtech, Inc." /t http://timestamp.verisign.com/scripts/timestamp.dll %TOP%\fscc.cat

exit