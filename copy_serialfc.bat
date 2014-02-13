set SERIALFC=..\serialfc\bin\serialfc-windows-2.4.5

copy %SERIALFC%\32\serialfc.inf redist\production\i386\serial\ /y
copy %SERIALFC%\32\serialfc.sys redist\production\i386\serial\ /y

copy %SERIALFC%\64\serialfc.inf redist\production\amd64\serial\ /y
copy %SERIALFC%\64\serialfc.sys redist\production\amd64\serial\ /y

xcopy %SERIALFC%\lib\* redist\production\serial\lib\ /e /i /y
xcopy %SERIALFC%\terminal\* redist\production\serial\terminal\ /e /i /y
xcopy %SERIALFC%\gui\* redist\production\serial\gui\ /e /i /y