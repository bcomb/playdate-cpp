set RUN_PROJECT=Physics

set VS_VCVARS_PATH="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo [INFO] Init Visual Studio env...
call %VS_VCVARS_PATH%

if errorlevel 1 (
    echo [ERROR] Can't find VCVARS.
    exit /b 1
)

echo [INFO] Generate project...
cmake -S . -B build_pd -DPDCPP_BUILD_EXAMPLES=ON -G "NMake Makefiles" --toolchain=%PLAYDATE_SDK_PATH%/C_API/buildsupport/arm.cmake -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 (
    echo [ERROR] CMake fail.
    exit /b 1
)

echo [INFO] Build...
pushd ~/dp0
cd build_pd
nmake clean && nmake
popd

if errorlevel 1 (
    echo [ERROR] Build fail.
    pause
    exit /b 1
)

echo [INFO] Deploy on Playdate...
PlaydateSimulator examples/%RUN_PROJECT%/%RUN_PROJECT%.pdx

