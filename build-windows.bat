@echo off
setlocal

rem ======== Config ========
set SDL2_DIR=SDL
set SDL2_BUILD_DIR=lib
set VS_GENERATOR=Visual Studio 17 2022
set ARCH=x64
set TOOLSET=ClangCL
rem ========================


cd "%SDL2_DIR%"

if exist "%SDL2_BUILD_DIR%" rmdir /s /q "%SDL2_BUILD_DIR%"
mkdir "%SDL2_BUILD_DIR%"

cmake -S . -B "%SDL2_BUILD_DIR%" ^
  -G "%VS_GENERATOR%" -A %ARCH% -T %TOOLSET% ^
  -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF

if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

rem Build Release
cmake --build "%SDL2_BUILD_DIR%" --config Release
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

rem Copy built libs and includes to dist
copy "%SDL2_BUILD_DIR%\Release\SDL2*.lib" "%SDL2_BUILD_DIR%\%ARCH%" >nul

cd ..
REM \ImGui\examples\example_sdl2_sdlrenderer2

clang-cl /Zi /MD /utf-8 ^
  /IImGui\examples\example_sdl2_sdlrenderer2\.. /IImGui\examples\example_sdl2_sdlrenderer2\..\.. /IImGui\examples\example_sdl2_sdlrenderer2\..\..\backends /I%SDL2_DIR%\include ^
  ImGui\examples\example_sdl2_sdlrenderer2\main.cpp ImGui\examples\example_sdl2_sdlrenderer2\..\..\backends\imgui_impl_sdl2.cpp ImGui\examples\example_sdl2_sdlrenderer2\..\..\backends\imgui_impl_sdlrenderer2.cpp ImGui\examples\example_sdl2_sdlrenderer2\..\..\imgui*.cpp ^
  /FeDebug\example_sdl2_sdlrenderer.exe /FoDebug\ ^
  /link /libpath:%SDL2_DIR%\%SDL2_BUILD_DIR%\%ARCH% SDL2-static.lib SDL2main.lib ^
  user32.lib gdi32.lib winmm.lib imm32.lib ole32.lib oleaut32.lib version.lib uuid.lib advapi32.lib setupapi.lib shell32.lib ^
  /subsystem:console


echo.
echo Build complete.
echo.

endlocal
