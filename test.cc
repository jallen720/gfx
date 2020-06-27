#include "ctk/ctk.h"
#include "gfx/gfx.h"

s32
main()
{
    gfx::state State = {};
    InitializeGLFWState(&State);
    InitializeVulkanState(&State);
    LoadAssets(&State);
    while(!gfx::WindowClosed(&State))
    {
        gfx::PollEvents(&State);
        gfx::Sleep(1);
    }
}
