#include <windows.h>
#include "gfx/graphics.h"
#include "ctk/data.h"

// Tests
#include "gfx/tests/default.h"
#include "gfx/tests/stencil.h"
#include "gfx/tests/transparency.h"

#define SELECT_TEST(NAME)\
    void (*test_create_state)(vulkan_instance *, assets *, vulkan_state *) = NAME##_test_create_state;\
    scene *(*test_create_scene)(assets *, vulkan_state *) = NAME##_test_create_scene;\
    void (*test_init)(vulkan_instance *, assets *, vulkan_state *, scene *) = NAME##_test_init;\
    void (*test_update)(vulkan_instance *, vulkan_state *, u32) = NAME##_test_update;

////////////////////////////////////////////////////////////
/// Internal
////////////////////////////////////////////////////////////
static void
update_input_state(input_state *InputState, GLFWwindow *Window)
{
    // Mouse Delta
    ctk::vec2 PreviousMousePosition = InputState->MousePosition;
    f64 CurrentMouseX = 0.0;
    f64 CurrentMouseY = 0.0;
    glfwGetCursorPos(Window, &CurrentMouseX, &CurrentMouseY);
    InputState->MousePosition = { CurrentMouseX, CurrentMouseY };

    // Calculate delta if previous position was not unset.
    if(PreviousMousePosition != GFX_UNSET_MOUSE_POSITION)
    {
        InputState->MouseDelta = InputState->MousePosition - PreviousMousePosition;
    }
}

static void
camera_controls(transform *CameraTransform, input_state *InputState)
{
    // Rotation
    if(InputState->MouseButtonDown[GLFW_MOUSE_BUTTON_2])
    {
        static const f32 SENS = 0.4f;
        CameraTransform->Rotation.X += InputState->MouseDelta.Y * SENS;
        CameraTransform->Rotation.Y -= InputState->MouseDelta.X * SENS;
        CameraTransform->Rotation.X = ctk::clamp(CameraTransform->Rotation.X, -80.0f, 80.0f);
    }

    // Translation
    ctk::vec3<f32> Translation = {};
    f32 Modifier = InputState->KeyDown[GLFW_KEY_LEFT_SHIFT] ? 4 :
                   InputState->KeyDown[GLFW_KEY_LEFT_CONTROL] ? 1 :
                   2;
    if(InputState->KeyDown[GLFW_KEY_W]) Translation.Z += 0.01f * Modifier;
    if(InputState->KeyDown[GLFW_KEY_S]) Translation.Z -= 0.01f * Modifier;
    if(InputState->KeyDown[GLFW_KEY_D]) Translation.X += 0.01f * Modifier;
    if(InputState->KeyDown[GLFW_KEY_A]) Translation.X -= 0.01f * Modifier;
    if(InputState->KeyDown[GLFW_KEY_E]) Translation.Y -= 0.01f * Modifier;
    if(InputState->KeyDown[GLFW_KEY_Q]) Translation.Y += 0.01f * Modifier;
    local_translate(CameraTransform, Translation);
}

struct test_state
{
    input_state InputState;
    window *Window;
    vulkan_instance *VulkanInstance;
    assets *Assets;
    vulkan_state *VulkanState;
    scene *Scene;
};

////////////////////////////////////////////////////////////
/// Main
////////////////////////////////////////////////////////////
s32
main()
{
    input_state InputState = {};
    window *Window = create_window(&InputState);
    vulkan_instance *VulkanInstance = create_vulkan_instance(Window);
    assets *Assets = create_assets(VulkanInstance);
    vulkan_state *VulkanState = create_vulkan_state(VulkanInstance, Assets);
    SELECT_TEST(transparency)
    test_create_state(VulkanInstance, Assets, VulkanState);
    scene *Scene = test_create_scene(Assets, VulkanState);
    test_init(VulkanInstance, Assets, VulkanState, Scene);
    while(!glfwWindowShouldClose(Window->Handle))
    {
        // Check if window should close.
        glfwPollEvents();
        if(InputState.KeyDown[GLFW_KEY_ESCAPE])
        {
            break;
        }

        // Frame processing.
        update_input_state(&InputState, Window->Handle);
        camera_controls(&Scene->Camera.Transform, &InputState);
        u32 SwapchainImageIndex = aquire_next_swapchain_image_index(VulkanInstance);
        update_uniform_data(VulkanInstance, Scene, SwapchainImageIndex);
        test_update(VulkanInstance, VulkanState, SwapchainImageIndex);
        Sleep(1);
    }
}
