@echo off
rem Build nativo de Windows con MSVC (Visual Studio Build Tools).
rem
rem   build.bat            release
rem   build.bat debug      con simbolos de depuracion
rem   build.bat clean      borra build\

setlocal enabledelayedexpansion
pushd "%~dp0"

set "MODE=%~1"
if "%MODE%"=="" set "MODE=release"

if /i "%MODE%"=="clean" (
    if exist build rmdir /s /q build
    echo Limpio.
    popd
    exit /b 0
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

set "VSPATH="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
)

if not defined VSPATH (
    echo No se encontro Visual Studio con las herramientas de C.
    echo Instala "Visual Studio Build Tools" con el workload de escritorio.
    popd
    exit /b 1
)

echo Usando: %VSPATH%

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo No se pudo preparar el entorno de compilacion.
    popd
    exit /b 1
)

if "%MODE%"=="debug" (
    set "OPT=/Od /Zi /D_DEBUG"
) else (
    set "OPT=/O2 /DNDEBUG"
)

if not exist build\obj mkdir build\obj

set "CFLAGS=/nologo /std:c11 /W3 /D_CRT_SECURE_NO_WARNINGS /ICommon /ISheet /ILang %OPT%"

rem El editor: TUI, la hoja y el lenguaje (sin el interprete).
echo Compilando el editor...
cl %CFLAGS% /Fobuild\obj\ /Febuild\celex.exe TUI\*.c Common\ref.c Sheet\sheet.c Sheet\order.c Lang\*.c
if errorlevel 1 goto :error

rem El interprete: la hoja, el lenguaje y los archivos comunes.
echo Compilando el interprete...
cl %CFLAGS% /Fobuild\obj\ /Febuild\celexs.exe Sheet\*.c Lang\*.c Common\*.c
if errorlevel 1 goto :error

if /i "%MODE%"=="test" goto :test

echo Listo: build\celex.exe y build\celexs.exe
popd
exit /b 0

:test
rem Las pruebas del lenguaje y de la hoja.
echo Compilando el runner de pruebas...
cl %CFLAGS% /Fobuild\obj\ /Febuild\runner.exe tests\runner.c
if errorlevel 1 goto :error

echo Listo: build\celex.exe y build\celexs.exe
build\runner.exe build\celexs.exe tests
if errorlevel 1 goto :fallo

popd
exit /b 0

:fallo
echo Fallaron las pruebas.
popd
exit /b 1

:error
echo Fallo la compilacion.
echo Si el error es LNK1104 hay una copia del editor corriendo: cierrala y
echo vuelve a intentarlo.
popd
exit /b 1
