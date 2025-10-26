#pragma once

//******************************************************************************
// Class definition
//******************************************************************************

// Helper class to "easily" render shadertoy-like shaders
// Convert result in grayscale and apply dithering for display on the Playdate screen
// Dithering bluenoise technique from arasp : https://aras-p.info/blog/2024/05/20/Crank-the-World-Playdate-demo/
class ShaderToy
{
public:
    ShaderToy() = default;
    ~ShaderToy() = default;

    void Initialize();
    void Finalize();

    void Render(float Time);
};