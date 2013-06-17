set SERIALFC=..\serialfc

copy %SERIALFC%\bin\serialfc\32\serialfc.inf redist\production\i386\serial\ /y
copy %SERIALFC%\bin\serialfc\32\serialfc.sys redist\production\i386\serial\ /y

copy %SERIALFC%\bin\serialfc\64\serialfc.inf redist\production\amd64\serial\ /y
copy %SERIALFC%\bin\serialfc\64\serialfc.sys redist\production\amd64\serial\ /y

xcopy %SERIALFC%\bin\serialfc\lib\* redist\production\serial\lib\ /e /i /y