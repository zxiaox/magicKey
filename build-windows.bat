@echo off
setlocal
cd /d "%~dp0"
title Build MagicKey
echo Building MagicKey for Windows x64...
echo.
echo Build log: "%CD%\build-windows.log"
> "build-windows.log" echo MagicKey build started %DATE% %TIME%
if errorlevel 1 (
    echo ERROR: Cannot write to this folder. Copy the project to a writable folder first.
    goto failed
)

rem Visual Studio's bundled CMake is not always available on the normal PATH.
set "MYWARKEY_CMAKE=cmake"
set "MYWARKEY_CTEST=ctest"
where cmake >nul 2>&1
if not errorlevel 1 goto found_cmake
set "MYWARKEY_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%MYWARKEY_VSWHERE%" goto missing_cmake
for /f "usebackq delims=" %%I in (`"%MYWARKEY_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "MYWARKEY_VS=%%I"
if not defined MYWARKEY_VS goto missing_cmake
set "MYWARKEY_CMAKE=%MYWARKEY_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "MYWARKEY_CTEST=%MYWARKEY_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
if not exist "%MYWARKEY_CMAKE%" goto missing_cmake

:found_cmake
rem Resolve CTest beside CMake when only CMake itself is on PATH.
if not "%MYWARKEY_CMAKE%"=="cmake" goto configure
for /f "delims=" %%I in ('where cmake') do if exist "%%~dpIctest.exe" set "MYWARKEY_CTEST=%%~dpIctest.exe"

:configure
echo [1/3] Configuring...
rem Use a Windows-only folder to avoid caches copied from macOS or Linux.
"%MYWARKEY_CMAKE%" -S . -B build-windows -A x64 >> "build-windows.log" 2>&1
if errorlevel 1 goto build_failed
echo [2/3] Compiling...
"%MYWARKEY_CMAKE%" --build build-windows --config Release >> "build-windows.log" 2>&1
if errorlevel 1 goto build_failed
echo [3/3] Testing...
"%MYWARKEY_CTEST%" --test-dir build-windows -C Release --output-on-failure >> "build-windows.log" 2>&1
if errorlevel 1 goto build_failed
if not exist "build-windows\Release\MagicKey.exe" (
    >> "build-windows.log" echo ERROR: The expected Windows executable was not produced.
    goto build_failed
)
echo.
type "build-windows.log"
echo.
echo SUCCESS: "%CD%\build-windows\Release\MagicKey.exe"
echo Double-click that executable to open MagicKey.
pause
exit /b 0

:missing_cmake
echo.
echo ERROR: CMake or the Visual C++ build tools could not be found.
echo Open Visual Studio Installer, choose Modify, and install:
echo   Desktop development with C++
echo   C++ CMake tools for Windows
echo   A Windows SDK
echo Then run this script again.
>> "build-windows.log" echo CMake or Visual C++ tools not found. Check the Visual Studio Installer components.
goto failed

:build_failed
echo.
type "build-windows.log"
echo.
echo BUILD FAILED. See the errors above and build-windows.log in this folder.
echo Check that Visual Studio has Desktop development with C++, CMake tools, and a Windows SDK.

:failed
echo.
pause
exit /b 1
