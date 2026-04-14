@echo off
setlocal

if "%~1"=="" (
    echo Usage: build_assimp.bat Debug^|Release
    exit /b 1
)

set CONFIG=%~1
set ROOT=%~dp0..
set ASSIMP_SRC=%ROOT%\external\assimp
set ASSIMP_BUILD=%ROOT%\external\assimp-build-windows
set ASSIMP_INSTALL=%ROOT%\external\assimp-install-windows

if /I "%CONFIG%"=="Debug" (
    if exist "%ASSIMP_INSTALL%\lib\assimpd.lib" exit /b 0
)

if /I "%CONFIG%"=="Release" (
    if exist "%ASSIMP_INSTALL%\lib\assimp.lib" exit /b 0
)

cmake -S "%ASSIMP_SRC%" -B "%ASSIMP_BUILD%" ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DCMAKE_INSTALL_PREFIX="%ASSIMP_INSTALL%" ^
  -DASSIMP_BUILD_TESTS=OFF ^
  -DASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
  -DASSIMP_INSTALL=ON

if errorlevel 1 exit /b 1

cmake --build "%ASSIMP_BUILD%" --config %CONFIG%
if errorlevel 1 exit /b 1

cmake --install "%ASSIMP_BUILD%" --config %CONFIG% --log-level=ERROR
if errorlevel 1 exit /b 1

endlocal
