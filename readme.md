# Playdate Cpp template

Fork from https://github.com/nstbayless/playdate-cpp

## Prerequisite
* Install the [arm-gnu-toolchain-14.3.rel1-mingw-w64-i686-arm-none-eabi](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain) and add to PATH (the gcc-arm-none-eabi-10.3-2021.10 toolchain bug at link stage)
* Install [PlaydateSDK](https://play.date/dev/) and add to PATH
* set PLAYDATE_SDK_PATH env var

## Generate Solution

### Generate msvc solution
>cmake -S . -B build_sim -DPDCPP_BUILD_EXAMPLES=ON

### Generate for the Playdate
From the `x64 Native Tools Command Prompt for VS 2022`
>cmake -S . -B build_pd -DPDCPP_BUILD_EXAMPLES=ON -G "NMake Makefiles"--toolchain=%PLAYDATE_SDK_PATH%/C_API/buildsupport/arm.cmake -DCMAKE_BUILD_TYPE=Release`

>cd build_pd
>
>nmake clean && nmake && PlaydateSimulator ..\examples\application\Application.pdx

### Deploy
From the emulator and only with the Playdate build do `Upload Game To Device`<br>
`Run Game On Device` crash dunno why


# Some examples

## ShaderToy
Play with "shader", try to copy paste "as it" some incredible shader from awesome [Shader Toy](https://www.shadertoy.com) community<br>
<br>
Use a blue noise dithering technique from [Aras Pranckevičius](https://aras-p.info/blog/2024/05/20/Crank-the-World-Playdate-demo/)

![Rubic's cube, remote](images/shadertoy.gif)
