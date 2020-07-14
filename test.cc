#include "gfx/gfx.h"

s32
main()
{
    gfx::input_state InputState = {};
    gfx::window *Window = gfx::CreateWindow_(&InputState);
    gfx::graphics_state *GraphicsState = gfx::CreateGraphicsState(Window, &InputState);
}
