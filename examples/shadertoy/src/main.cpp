/**
 * Hello World CPP
 *
 * Reproduces the Hello World Example from the Playdate C SDK, but with C++
 * instead. A full game can be built from similar principles.
 *
 * MrBZapp 9/2023
 */
#include <pdcpp/pdnewlib.h>
#include "Application.h"
#include "Globals.h"

/**
 * The Playdate API requires a C-style, or static function to be called as the
 * main update function. Here we use such a function to delegate execution to
 * our class.
 */
static int gameTick(void* userdata)
{
    _G.App->Update();
    return 1;
};

/**
 * the `eventHandler` function is the entry point for all Playdate applications
 * and Lua extensions. It requires C-style linkage (no name mangling) so we
 * must wrap the entire thing in this `extern "C" {` block
 */
#ifdef __cplusplus
extern "C" {
#endif

    /**
     * This is the main event handler. Using the `Init` and `Terminate` events, we
     * allocate and free the `HelloWorld` handler accordingly.
     *
     * You only need this `_WINDLL` block if you want to run this on a simulator on
     * a windows machine, but for the sake of broad compatibility, we'll leave it
     * here.
     */
#ifdef _WINDLL
    __declspec(dllexport)
#endif
    int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
    {
        /*
         * This is required, otherwise linker errors abound
         */
        eventHandler_pdnewlib(pd, event, arg);

        // Initialization just creates our "game" object
        if (event == kEventInit)
        {
            _G.pd = pd;
            pd->display->setRefreshRate(0);
            
            _G.App = new Application(pd);
            _G.App->Initialize();

            // Calling setUpdateCallback in Init will run app in full C (no Lua mode)
            pd->system->setUpdateCallback(gameTick, pd);
        }

        // Destroy the global state to prevent memory leaks
        if (event == kEventTerminate)
        {
            pd->system->logToConsole("Tearing down...");
            _G.App->Finalize();
            delete _G.App;
            _G.App = nullptr;
        }
        return 0;
    }
#ifdef __cplusplus
}
#endif
