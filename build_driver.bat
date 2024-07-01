set TOP=tmp\%4\%3
set NAME=fscc

echo off
call C:\WinDDK\7600.16385.1\bin\setenv.bat C:\WinDDK\7600.16385.1\ %~1
cd %~dp0

:building_driver
echo Building Driver...
pushd src\
build -cfeg
if %errorlevel% neq 0 exit /b %errorlevel%
popd

pushd src\classinstaller\
build -cfeg
if %errorlevel% neq 0 exit /b %errorlevel%
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
copy src\%2\%3\filter.inf %TOP%\ > nul

:copy_pdb_files
echo Copying Debugging Files...
copy src\%2\%3\fscc.pdb %TOP%\ > nul

:copy_coinstaller_files
echo Copying Coinstaller Files...
copy redist\%4\%3\WdfCoInstaller* %TOP%\ > nul
copy src\classinstaller\%2\%3\fscc.dll %TOP%\ > nul

:copy_serial_files
echo Copying Serial Files...
copy redist\%4\%3\serial\serialfc.inf %TOP%\ > nul
copy redist\%4\%3\serial\serialfc.sys %TOP%\ > nul

:create_catalogs
echo Creating Driver Catalogs...
Inf2cat.exe /driver:%TOP%\ /os:%5 > nul
if %errorlevel% neq 0 exit /b %errorlevel%

:sign_files
:echo Signing Files...
:signtool sign /ac "DigiCert High Assurance EV Root CA.crt" /n "Commtech, Inc." /t http://timestamp.digicert.com/ /sha1 A3B9E0686AAD8D1E42216AC6C701766E9D1AA712 %TOP%\fastcom.cat
:signtool sign /ac "DigiCert High Assurance EV Root CA.crt" /n "Commtech, Inc." /t http://timestamp.digicert.com/ /sha1 A55F5C61CE305CD8F45E6C46AD704C452FBF630E %TOP%\fastcom.cat
:if %errorlevel% neq 0 exit /b %errorlevel%

: As of 08/04/22
signtool sign /fd SHA512 /t http://timestamp.digicert.com/ /sha1 b757f8701a8ce35e8a243a12207ce8a697756df0 %TOP%\fastcom.cat
if %errorlevel% neq 0 exit /b %errorlevel%

exit