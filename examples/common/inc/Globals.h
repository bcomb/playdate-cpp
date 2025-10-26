#pragma once
#include <stdint.h>

//******************************************************************************
// Forward declarations
//******************************************************************************
struct PlaydateAPI;
class Application;

//******************************************************************************
// Class definition
//******************************************************************************
struct Globals
{
    PlaydateAPI* pd = nullptr;
    Application* App = nullptr;
};

extern Globals _G;