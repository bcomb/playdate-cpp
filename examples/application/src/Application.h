#pragma once

//******************************************************************************
// Forward declarations
//******************************************************************************
struct PlaydateAPI;
struct LCDFont;

//******************************************************************************
// Class declaration
//******************************************************************************

/**
 * This Class contains the entire logic of the "game"
 */
class Application
{
public:
    explicit Application(PlaydateAPI* api);
    void initialize();
    void update();
    void finalize();

private:
    PlaydateAPI* pd = nullptr;
    const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
    LCDFont* font = nullptr;
};
