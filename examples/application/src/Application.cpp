#include "Application.h"
#include <pdcpp/pdnewlib.h>

/******************************************************************************/
Application::Application(PlaydateAPI* api)
: pd(api)
{
}

/******************************************************************************/
void Application::initialize()
{
    // Max framerate (aka 50Hz)
    pd->display->setRefreshRate(0);

    // Default font
    const char* err;
    font = pd->graphics->loadFont(fontpath, &err);

    if (font == nullptr)
    {
        pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);
    }
}

/******************************************************************************/
void Application::finalize()
{

}

/******************************************************************************/
void Application::update()
{
    pd->graphics->clear(kColorWhite);
    pd->graphics->setFont(font);
    pd->graphics->drawText("Hello C++!", strlen("Hello World!"), kASCIIEncoding, 32, 32);
    pd->system->drawFPS(0, 0);
}