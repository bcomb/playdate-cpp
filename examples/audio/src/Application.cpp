#include "Application.h"
#include "ImageLoader.h"
#include "Audio.h"

#include <pd_api.h>
#include <math.h>
#include <assert.h>

//******************************************************************************
Application::Application(PlaydateAPI* api)
: pd(api)
{
}

//******************************************************************************
void Application::Initialize()
{        
    // Max framerate (aka 50Hz)
    pd->display->setRefreshRate(0);

    // Default font
    const char* Err;
    Font = pd->graphics->loadFont(FontPath, &Err);

    if (Font == nullptr)
    {
        pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, FontPath, Err);
    }

    pd->system->resetElapsedTime();
}

//******************************************************************************
void Application::Finalize()
{
}












//******************************************************************************
void Application::Update()
{    
    GlobalTime = pd->system->getElapsedTime();
    pd->graphics->clear(kColorWhite);

    test(GlobalTime);

    pd->graphics->setFont(Font);
    pd->system->drawFPS(0, 240-14);

    ++FrameCount;
}