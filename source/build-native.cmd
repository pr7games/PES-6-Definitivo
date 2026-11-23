@echo off
pushd "%~dp0"
setlocal
set "TOOLCHAIN_ROOT=F:\Proyectos Port\Pruebas\fifa08-research - copia (2)\tools"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "PATH=%TOOLCHAIN_ROOT%\llvm\bin;%TOOLCHAIN_ROOT%\cmake-4.4.4\cmake-4.4.4-windows-x86_64\bin;C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

node "%~dp0scripts\patch-generated.cjs"
if errorlevel 1 exit /b 1

cmake -S "%~dp0native" -B "%~dp0native\build" -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%TOOLCHAIN_ROOT%\rexglue-0.10.0\win-amd64" -DREXGLUE_RECOMP_DEBUG_INFO=none
if errorlevel 1 exit /b 1

cmake --build "%~dp0native\build" --parallel 6 %*
if errorlevel 1 exit /b 1

if not exist "%~dp0..\bin\native\build" mkdir "%~dp0..\bin\native\build"
copy /y "%~dp0native\build\pes6.exe" "%~dp0..\bin\native\build\pes6.exe" >nul
copy /y "%~dp0native\build\pes6.exe" "%~dp0..\bin\native\build\pes6-x.exe" >nul
if exist "%~dp0native\replay-runtime\rexruntime.dll" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\deploy-replay-runtime.ps1"
    if errorlevel 1 exit /b 1
)

if exist "%~dp0native\extension\thirdparty\minhook\CMakeLists.txt" (
    call "%~dp0build-native-extension.cmd"
    if errorlevel 1 exit /b 1
    node "%~dp0scripts\stage-native-extension.cjs" --install
    if errorlevel 1 exit /b 1
)

echo Build succeeded!
exit /b 0
