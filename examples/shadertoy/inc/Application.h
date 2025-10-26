#pragma once

#include <stdint.h>

//******************************************************************************
// Forward declarations
//******************************************************************************
struct PlaydateAPI;
struct LCDFont;
class ShaderToy;

//******************************************************************************
// Class definition
//******************************************************************************

// This Class contains the entire logic of the "game"
class Application
{
public: 
    explicit Application(PlaydateAPI* api);
    void Initialize();
    void Update();
    void Finalize();

    // Reference time of the application in seconds
    inline float GetGlobalTime() const { return GlobalTime;  }
    // Number of frames since application started
    inline uint32_t GetFrameCount() const { return FrameCount; }

private:
    PlaydateAPI* pd = nullptr;
    const char* FontPath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
    LCDFont* Font = nullptr;
    float GlobalTime = 0.0f;
    uint32_t FrameCount = 0;

    ShaderToy* Shadertoy = nullptr;
};
