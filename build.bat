@echo off
setlocal enabledelayedexpansion

:: ============================================
:: FrameGen - Automatic Build Script
:: ============================================

title FrameGen Build System

echo.
echo ========================================
echo FrameGen Build System v1.0
echo Frame Generation for DirectX 11 Games
echo ========================================
echo.

:: Settings
set "PROJECT_DIR=%~dp0"
set "BUILD_DIR=%PROJECT_DIR%build"
set "DEPS_DIR=%PROJECT_DIR%deps"
set "OUTPUT_DIR=%PROJECT_DIR%output"

:: Create directories
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%DEPS_DIR%" mkdir "%DEPS_DIR%"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo [1/5] Checking dependencies...
echo.

:: Check PowerShell
where powershell >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: PowerShell not found!
    pause
    exit /b 1
)

:: Download MinHook
set "MINHOOK_DIR=%DEPS_DIR%\minhook"
if not exist "%MINHOOK_DIR%\include\MinHook.h" (
    echo Downloading MinHook...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/TsudaKageyu/minhook/archive/refs/heads/master.zip' -OutFile '%DEPS_DIR%\minhook.zip'"
    if exist "%DEPS_DIR%\minhook.zip" (
        powershell -Command "Expand-Archive -Path '%DEPS_DIR%\minhook.zip' -DestinationPath '%DEPS_DIR%' -Force"
        if exist "%DEPS_DIR%\minhook-master" (
            move "%DEPS_DIR%\minhook-master" "%MINHOOK_DIR%" >nul
        )
        del "%DEPS_DIR%\minhook.zip" >nul 2>&1
    )
)

:: Download ImGui
set "IMGUI_DIR=%DEPS_DIR%\imgui"
if not exist "%IMGUI_DIR%\imgui.h" (
    echo Downloading ImGui...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/ocornut/imgui/archive/refs/heads/master.zip' -OutFile '%DEPS_DIR%\imgui.zip'"
    if exist "%DEPS_DIR%\imgui.zip" (
        powershell -Command "Expand-Archive -Path '%DEPS_DIR%\imgui.zip' -DestinationPath '%DEPS_DIR%' -Force"
        if exist "%DEPS_DIR%\imgui-master" (
            move "%DEPS_DIR%\imgui-master" "%IMGUI_DIR%" >nul
        )
        del "%DEPS_DIR%\imgui.zip" >nul 2>&1
    )
)

:: Check if files downloaded
if not exist "%MINHOOK_DIR%\include\MinHook.h" (
    echo ERROR: MinHook not downloaded! Check internet connection.
    pause
    exit /b 1
)

if not exist "%IMGUI_DIR%\imgui.h" (
    echo ERROR: ImGui not downloaded! Check internet connection.
    pause
    exit /b 1
)

echo [OK] Dependencies ready
echo.

echo [2/5] Finding Visual Studio...
echo.

:: Find Visual Studio
set "VS_PATH="
set "VS_VERSION="

:: Try vswhere first
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

:: Manual search if vswhere failed
if not defined VS_PATH (
    :: Visual Studio 2022
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community" (
        set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
        set "VS_VERSION=2022"
    )
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional" (
        set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
        set "VS_VERSION=2022"
    )
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise" (
        set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
        set "VS_VERSION=2022"
    )
)

if not defined VS_PATH (
    :: Visual Studio 2019
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community" (
        set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community"
        set "VS_VERSION=2019"
    )
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional" (
        set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional"
        set "VS_VERSION=2019"
    )
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise" (
        set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise"
        set "VS_VERSION=2019"
    )
)

if not defined VS_PATH (
    :: Visual Studio 2017
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community" (
        set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community"
        set "VS_VERSION=2017"
    )
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional" (
        set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional"
        set "VS_VERSION=2017"
    )
)

if not defined VS_PATH (
    echo ERROR: Visual Studio not found!
    echo.
    echo Please install Visual Studio 2017/2019/2022 with:
    echo   - Desktop development with C++
    echo   - Windows 10/11 SDK
    echo.
    pause
    exit /b 1
)

echo Found Visual Studio: !VS_VERSION! in "!VS_PATH!"

:: Find vcvarsall.bat
set "VCVARSALL="
if exist "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARSALL=!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "!VS_PATH!\VC\vcvarsall.bat" (
    set "VCVARSALL=!VS_PATH!\VC\vcvarsall.bat"
)

if not defined VCVARSALL (
    echo ERROR: vcvarsall.bat not found!
    pause
    exit /b 1
)

echo [OK] Visual Studio found
echo.

echo [3/5] Preparing source files...
echo.

:: Setup Visual Studio environment
call "!VCVARSALL!" x64

echo [OK] Source files ready
echo.

echo [4/5] Compiling...
echo.

:: Compiler flags
set "CL_FLAGS=/nologo /EHsc /W3 /O2 /MD /DNDEBUG /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE"
set "CL_FLAGS=%CL_FLAGS% /D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS"
set "CL_FLAGS=%CL_FLAGS% /DNOMINMAX"

:: Include paths
set "INCLUDES=/I"%PROJECT_DIR%include""
set "INCLUDES=%INCLUDES% /I"%PROJECT_DIR%src""
set "INCLUDES=%INCLUDES% /I"%IMGUI_DIR%""
set "INCLUDES=%INCLUDES% /I"%IMGUI_DIR%\backends""
set "INCLUDES=%INCLUDES% /I"%MINHOOK_DIR%\include""
set "INCLUDES=%INCLUDES% /I"%MINHOOK_DIR%\src""
set "INCLUDES=%INCLUDES% /I"%MINHOOK_DIR%\src\hde""

:: Linker flags
set "LINK_FLAGS=/DLL /OUT:"%OUTPUT_DIR%\d3d11.dll""

:: Libraries
set "LIBS=d3d11.lib dxgi.lib user32.lib gdi32.lib"

:: Source files - main
set "SRC_FILES=%PROJECT_DIR%src\main.cpp"
set "SRC_FILES=%SRC_FILES% %PROJECT_DIR%src\d3d11_hook.cpp"
set "SRC_FILES=%SRC_FILES% %PROJECT_DIR%src\frame_interpolator.cpp"
set "SRC_FILES=%SRC_FILES% %PROJECT_DIR%src\menu.cpp"
set "SRC_FILES=%SRC_FILES% %PROJECT_DIR%src\input_manager.cpp"

:: ImGui source files
set "SRC_FILES=%SRC_FILES% %IMGUI_DIR%\imgui.cpp"
set "SRC_FILES=%SRC_FILES% %IMGUI_DIR%\imgui_draw.cpp"
set "SRC_FILES=%SRC_FILES% %IMGUI_DIR%\imgui_tables.cpp"
set "SRC_FILES=%SRC_FILES% %IMGUI_DIR%\imgui_widgets.cpp"
set "SRC_FILES=%SRC_FILES% %IMGUI_DIR%\backends\imgui_impl_dx11.cpp"
set "SRC_FILES=%SRC_FILES% %IMGUI_DIR%\backends\imgui_impl_win32.cpp"

:: MinHook wrapper (includes C files with extern "C")
set "SRC_FILES=%SRC_FILES% %PROJECT_DIR%src\minhook_wrapper.cpp"

:: Export definition
set "EXPORT_DEF=/DEF:%PROJECT_DIR%src\exports.def"

:: Compile
echo Compiling...
cl %CL_FLAGS% %INCLUDES% /LD %SRC_FILES% /link %LINK_FLAGS% %LIBS% %EXPORT_DEF%

if %ERRORLEVEL% neq 0 (
    echo.
    echo ========================================
    echo COMPILATION ERROR!
    echo ========================================
    echo.
    echo Check the errors above and fix them.
    echo.
    pause
    exit /b 1
)

echo.
echo [5/5] Cleanup...
echo.

:: Delete temporary files
del /q *.obj >nul 2>&1
del /q *.pdb >nul 2>&1
del /q *.idb >nul 2>&1
del /q *.ilk >nul 2>&1
del /q *.exp >nul 2>&1
del /q "%OUTPUT_DIR%\*.lib" >nul 2>&1

:: Check if DLL was created
if exist "%OUTPUT_DIR%\d3d11.dll" (
    echo.
    echo ========================================
    echo SUCCESS!
    echo ========================================
    echo.
    echo File created: %OUTPUT_DIR%\d3d11.dll
    echo.
    echo Instructions:
    echo 1. Copy d3d11.dll to game folder
    echo 2. Run the game
    echo 3. Press DEL to open menu
    echo 4. Press INS to toggle frame generation
    echo.
    for %%I in ("%OUTPUT_DIR%\d3d11.dll") do echo File size: %%~zI bytes
    echo.
) else (
    echo.
    echo ERROR: DLL was not created!
    echo.
)

pause
exit /b 0
