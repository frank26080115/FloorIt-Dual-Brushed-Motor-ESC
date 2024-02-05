cd ..
make clean all
if not exist "release" mkdir "release"
rm -rf release
cd tools
python fw-post-processor.py
xcopy hydra-fw-tool.py ..\release\ /Y
pyinstaller37 -F --exclude-module _bootlocale --hidden-import=pkg_resources.py2_warn hydra-fw-tool.py
xcopy dist\hydra-fw-tool.exe ..\release\ /Y
cd ../bin
xcopy *.hex ..\release /Y /s
xcopy with-bootloader ..\release\with-bootloader /I /Y
cd ../tools
pause
