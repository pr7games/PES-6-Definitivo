@echo off
pushd "%~dp0"
setlocal
set "EXTENSION_TOOLS=F:\Proyectos Port\Pruebas\fifa08-research - copia (2)\tools"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "PATH=%EXTENSION_TOOLS%\llvm\bin;%EXTENSION_TOOLS%\cmake-4.4.4\cmake-4.4.4-windows-x86_64\bin;C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
node "%~dp0scripts\generate-native-extension.cjs"
if errorlevel 1 exit /b 1
node "%~dp0scripts\compile-native-present.cjs"
if errorlevel 1 exit /b 1
cmake -S "%~dp0native\extension" -B "%~dp0native\extension\build" -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%EXTENSION_TOOLS%\rexglue-0.10.0\win-amd64"
if errorlevel 1 exit /b 1
cmake --build "%~dp0native\extension\build" --parallel 4
exit /b %errorlevel%
