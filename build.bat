@echo off
rem Build nativo de Windows con MSVC (Visual Studio Build Tools).
rem
rem   build.bat            release
rem   build.bat debug      con simbolos de depuracion
rem   build.bat test      compila y corre las pruebas
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
    set "OPT=/Od /Zi /D_DEBUG /DCELEX_DEBUG"
) else (
    set "OPT=/O2 /DNDEBUG"
)

if not exist build\obj mkdir build\obj

rem El numero de compilacion va en la barra del editor, para saber siempre con
rem que binario se esta trabajando.
set "BUILD_ID="
for /f "usebackq tokens=*" %%i in (`git rev-parse --short HEAD 2^>nul`) do set "BUILD_ID=%%i"
if not defined BUILD_ID set "BUILD_ID=dev"
rem Si hay cambios sin commitear, se marca con un mas.
set "DIRTY="
for /f "usebackq tokens=*" %%i in (`git status --porcelain 2^>nul`) do set "DIRTY=1"
if defined DIRTY set "BUILD_ID=%BUILD_ID%+"
>build\version.h echo // Generado: no editar.
>>build\version.h echo #define CELEX_BUILD "%BUILD_ID%"
echo Compilacion: %BUILD_ID%

rem /utf-8 porque los fuentes estan en UTF-8, con los acentos y los
rem caracteres de la caja.
set "CFLAGS=/nologo /std:c11 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /ICommon /ISheet /ILang /Ibuild %OPT%"

rem El editor: TUI, la hoja y el lenguaje (sin el interprete).
echo Compilando el editor...
for /l %%i in (1,1,4) do (
    cl %CFLAGS% /Fobuild\obj\ /Febuild\celex.exe TUI\*.c Common\ref.c Sheet\sheet.c Sheet\order.c Lang\*.c /link user32.lib
    if not errorlevel 1 goto :editor_listo
    echo Reintentando en dos segundos...
    ping -n 3 127.0.0.1 >nul
)
goto :error
:editor_listo

rem El interprete: la hoja, el lenguaje y los archivos comunes.
echo Compilando el interprete...
for /l %%i in (1,1,4) do (
    cl %CFLAGS% /Fobuild\obj\ /Febuild\celexs.exe Sheet\*.c Lang\*.c Common\*.c
    if not errorlevel 1 goto :interprete_listo
    echo Reintentando en dos segundos...
    ping -n 3 127.0.0.1 >nul
)
goto :error
:interprete_listo

if /i "%MODE%"=="test" goto :test

echo Listo: build\celex.exe y build\celexs.exe
popd
exit /b 0

:test
rem Las pruebas del lenguaje y de la hoja.
echo Compilando el runner de pruebas...
for /l %%i in (1,1,4) do (
    cl %CFLAGS% /Fobuild\obj\ /Febuild\runner.exe tests\runner.c
    if not errorlevel 1 goto :runner_listo
    echo Reintentando en dos segundos...
    ping -n 3 127.0.0.1 >nul
)
goto :error
:runner_listo

rem Las pruebas de edicion: usan el gestor de celdas, pero no el programa.
set "TUI_SOURCES="
for %%f in (TUI\*.c) do (
    if /i not "%%f"=="TUI\main.c" set "TUI_SOURCES=!TUI_SOURCES! %%f"
)

echo Compilando las pruebas de edicion...
for /l %%i in (1,1,4) do (
    cl %CFLAGS% /ITUI /Fobuild\obj\ /Febuild\editor_test.exe tests\editor_test.c !TUI_SOURCES! Common\ref.c Sheet\sheet.c Sheet\order.c Lang\*.c /link user32.lib
    if not errorlevel 1 goto :editor_listo
    echo Reintentando en dos segundos...
    ping -n 3 127.0.0.1 >nul
)
goto :error
:editor_listo

rem Las pruebas de las teclas: manejan las teclas como el programa de verdad.
echo Compilando las pruebas de las teclas...
for /l %%i in (1,1,4) do (
    cl %CFLAGS% /ITUI /Fobuild\obj\ /Febuild\keys_test.exe tests\keys_test.c !TUI_SOURCES! Common\ref.c Sheet\sheet.c Sheet\order.c Lang\*.c /link user32.lib
    if not errorlevel 1 goto :teclas_listas
    echo Reintentando en dos segundos...
    ping -n 3 127.0.0.1 >nul
)
goto :error
:teclas_listas

rem Las pruebas de la entrada: convierten lo que llega de la terminal en
rem teclas y en pegados.
echo Compilando las pruebas de la entrada...
for /l %%i in (1,1,4) do (
    cl %CFLAGS% /ITUI /Fobuild\obj\ /Febuild\input_test.exe tests\input_test.c !TUI_SOURCES! Common\ref.c Sheet\sheet.c Sheet\order.c Lang\*.c /link user32.lib
    if not errorlevel 1 goto :entrada_lista
    echo Reintentando en dos segundos...
    ping -n 3 127.0.0.1 >nul
)
goto :error
:entrada_lista

rem Las pruebas del editor entero: corren el programa con las teclas escritas
rem en un archivo.
echo Compilando las pruebas del editor...
for /l %%i in (1,1,4) do (
    cl %CFLAGS% /Fobuild\obj\ /Febuild\app_test.exe tests\app_test.c /link user32.lib
    if not errorlevel 1 goto :app_listo
    echo Reintentando en dos segundos...
    ping -n 3 127.0.0.1 >nul
)
goto :error
:app_listo

rem Las pruebas de la consola: corren el programa dentro de una consola de
rem verdad, que es como se usa todos los dias.
echo Compilando las pruebas de la consola...
for /l %%i in (1,1,4) do (
    cl %CFLAGS% /Fobuild\obj\ /Febuild\console_test.exe tests\console_test.c /link user32.lib
    if not errorlevel 1 goto :consola_lista
    echo Reintentando en dos segundos...
    ping -n 3 127.0.0.1 >nul
)
goto :error
:consola_lista

echo Listo: build\celex.exe y build\celexs.exe
build\runner.exe build\celexs.exe tests
if errorlevel 1 goto :fallo

build\editor_test.exe
if errorlevel 1 goto :fallo

build\keys_test.exe
if errorlevel 1 goto :fallo

build\input_test.exe
if errorlevel 1 goto :fallo

build\app_test.exe build\celex.exe
if errorlevel 1 goto :fallo

build\console_test.exe build\celex.exe
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
