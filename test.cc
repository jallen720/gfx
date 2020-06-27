#include "ctk/ctk.h"
#include "gfx/gfx.h"

s32
main()
{
    gfx::limits Limits = {};
    Limits.DeviceMemorySize = 2 * gfx::MEGABYTE;
    Limits.HostMemorySize = 2 * gfx::MEGABYTE;
    gfx::state *GFXState = gfx::Initialize(&Limits);
    while(!gfx::WindowClosed(GFXState))
    {
        gfx::PollEvents(GFXState);
        gfx::Sleep(1);
    }
}
