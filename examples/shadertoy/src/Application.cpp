#include "Application.h"
#include "ImageLoader.h"
#include "Shadertoy.h"

#include <pdcpp/pdnewlib.h>
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

    Shadertoy = new ShaderToy();
    Shadertoy->Initialize();
}

//******************************************************************************
void Application::Finalize()
{
    Shadertoy->Finalize();
    delete Shadertoy;
    Shadertoy = nullptr;
}

//******************************************************************************
void Application::Update()
{    
    GlobalTime = pd->system->getElapsedTime();
    pd->graphics->clear(kColorWhite);

    Shadertoy->Render(GlobalTime);

    pd->graphics->setFont(Font);
    pd->system->drawFPS(0, 0);

    ++FrameCount;
}