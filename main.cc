#include <windows.h>
#include "gfx/graphics.h"
#include "ctk/data.h"

////////////////////////////////////////////////////////////
/// Internal
////////////////////////////////////////////////////////////
static ctk::vec3<f32>
load_vec3(ctk::data *Data)
{
    return { ctk::to_f32(Data, 0u), ctk::to_f32(Data, 1u), ctk::to_f32(Data, 2u) };
}

static transform
load_transform(ctk::data *Data)
{
    transform Transform = {};
    if(Data->Children.Count == 0)
    {
        Transform.Scale = { 1, 1, 1 };
    }
    else
    {
        Transform.Position = load_vec3(ctk::at(Data, "position"));
        Transform.Rotation = load_vec3(ctk::at(Data, "rotation"));
        Transform.Scale = load_vec3(ctk::at(Data, "scale"));
    }
    return Transform;
}

static scene *
create_scene(vulkan_state *VulkanState, assets *Assets, cstr Path)
{
    scene *Scene = ctk::allocate<scene>();
    *Scene = {};

    ctk::data SceneData = ctk::load_data(Path);

    // Camera
    ctk::data *CameraData = ctk::at(&SceneData, "camera");
    Scene->Camera.Transform = load_transform(ctk::at(CameraData, "transform"));
    Scene->Camera.FieldOfView = ctk::to_f32(CameraData, "field_of_view");

    // Entities
    ctk::data *EntityMap = ctk::at(&SceneData, "entities");
    for(u32 EntityIndex = 0; EntityIndex < EntityMap->Children.Count; ++EntityIndex)
    {
        ctk::data *EntityData = ctk::at(EntityMap, EntityIndex);
        entity *Entity = ctk::push(&Scene->Entities, EntityData->Key.Data);
        ctk::push(&Scene->EntityUBOs);

        // Transform
        Entity->Transform = load_transform(ctk::at(EntityData, "transform"));

        // Descriptor Sets (optional)
        ctk::data *DescriptorSetArray = ctk::find(EntityData, "descriptor_sets");
        if(DescriptorSetArray)
        {
            for(u32 DescriptorSetIndex = 0; DescriptorSetIndex < DescriptorSetArray->Children.Count; ++DescriptorSetIndex)
            {
                ctk::push(&Entity->DescriptorSets, ctk::at(&VulkanState->DescriptorSets, ctk::to_cstr(DescriptorSetArray,
                                                                                                      DescriptorSetIndex)));
            }
        }

        // Graphics Pipeline (optional)
        ctk::data *GraphicsPipelineName = ctk::find(EntityData, "graphics_pipeline");
        if(GraphicsPipelineName)
        {
            Entity->GraphicsPipeline = ctk::at(&VulkanState->GraphicsPipelines, ctk::to_cstr(GraphicsPipelineName));
        }

        // Mesh
        Entity->Mesh = ctk::at(&Assets->Meshes, ctk::to_cstr(EntityData, "mesh"));
    }
    Scene->EntityUniformBuffer = ctk::at(&VulkanState->UniformBuffers, "entity");

    // Cleanup
    ctk::_free(&SceneData);

    return Scene;
}

static void
local_translate(transform *Transform, ctk::vec3<f32> Translation)
{
    ctk::vec3<f32> *Position = &Transform->Position;
    ctk::vec3<f32> *Rotation = &Transform->Rotation;

    glm::mat4 WorldMatrix(1.0f);
    WorldMatrix = glm::rotate(WorldMatrix, glm::radians(Rotation->X), { 1.0f, 0.0f, 0.0f });
    WorldMatrix = glm::rotate(WorldMatrix, glm::radians(Rotation->Y), { 0.0f, 1.0f, 0.0f });
    WorldMatrix = glm::rotate(WorldMatrix, glm::radians(Rotation->Z), { 0.0f, 0.0f, 1.0f });
    WorldMatrix = glm::translate(WorldMatrix, { Position->X, Position->Y, Position->Z });

    ctk::vec3<f32> Right = {};
    Right.X = WorldMatrix[0][0];
    Right.Y = WorldMatrix[1][0];
    Right.Z = WorldMatrix[2][0];

    ctk::vec3<f32> Up = {};
    Up.X = WorldMatrix[0][1];
    Up.Y = WorldMatrix[1][1];
    Up.Z = WorldMatrix[2][1];

    ctk::vec3<f32> Forward = {};
    Forward.X = WorldMatrix[0][2];
    Forward.Y = WorldMatrix[1][2];
    Forward.Z = WorldMatrix[2][2];

    ctk::vec3<f32> NewPosition = *Position;
    NewPosition = NewPosition + (Right * Translation.X);
    NewPosition = NewPosition + (Up * Translation.Y);
    *Position = NewPosition + (Forward * Translation.Z);
}

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
    scene *Scene = create_scene(VulkanState, Assets, "assets/scenes/test_scene.ctkd");

    ////////////////////////////////////////////////////////////
    /// Example
    ////////////////////////////////////////////////////////////
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    // Stencil Render GP
    vtk::graphics_pipeline_info StencilRenderGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&StencilRenderGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "stencil_render_vert"));
    ctk::push(&StencilRenderGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "stencil_render_frag"));
    ctk::push(&StencilRenderGPInfo.DescriptorSetLayouts, ctk::at(&VulkanState->DescriptorSets, "entity")->Layout);
    ctk::push(&StencilRenderGPInfo.DescriptorSetLayouts, ctk::at(&VulkanState->DescriptorSets, "grass_texture")->Layout);
    ctk::push(&StencilRenderGPInfo.VertexInputs, { 0, 0, *ctk::at(&VulkanState->VertexAttributeIndexes, "position") });
    ctk::push(&StencilRenderGPInfo.VertexInputs, { 0, 1, *ctk::at(&VulkanState->VertexAttributeIndexes, "uv") });
    StencilRenderGPInfo.VertexLayout = &VulkanState->VertexLayout;
    ctk::push(&StencilRenderGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&StencilRenderGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });

    StencilRenderGPInfo.RasterizationState.cullMode = VK_CULL_MODE_NONE;

    StencilRenderGPInfo.DepthStencilState.stencilTestEnable = VK_TRUE;
    StencilRenderGPInfo.DepthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    StencilRenderGPInfo.DepthStencilState.back.failOp = VK_STENCIL_OP_REPLACE;
    StencilRenderGPInfo.DepthStencilState.back.depthFailOp = VK_STENCIL_OP_REPLACE;
    StencilRenderGPInfo.DepthStencilState.back.passOp = VK_STENCIL_OP_REPLACE;
    StencilRenderGPInfo.DepthStencilState.back.compareMask = 0xFF;
    StencilRenderGPInfo.DepthStencilState.back.writeMask = 0xFF;
    StencilRenderGPInfo.DepthStencilState.back.reference = 1;
    StencilRenderGPInfo.DepthStencilState.front = StencilRenderGPInfo.DepthStencilState.back;

    StencilRenderGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    StencilRenderGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    StencilRenderGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    ctk::push(&VulkanState->GraphicsPipelines, "stencil_render",
              vtk::create_graphics_pipeline(VulkanInstance->Device.Logical, ctk::at(&VulkanState->RenderPasses, "direct"), &StencilRenderGPInfo));

    // Outline GP
    vtk::graphics_pipeline_info OutlineGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&OutlineGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "outline_vert"));
    ctk::push(&OutlineGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "outline_frag"));
    ctk::push(&OutlineGPInfo.DescriptorSetLayouts, ctk::at(&VulkanState->DescriptorSets, "entity")->Layout);
    ctk::push(&OutlineGPInfo.VertexInputs, { 0, 0, *ctk::at(&VulkanState->VertexAttributeIndexes, "position") });
    ctk::push(&OutlineGPInfo.VertexInputs, { 0, 1, *ctk::at(&VulkanState->VertexAttributeIndexes, "normal") });
    OutlineGPInfo.VertexLayout = &VulkanState->VertexLayout;
    ctk::push(&OutlineGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&OutlineGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });

    OutlineGPInfo.RasterizationState.cullMode = VK_CULL_MODE_NONE;

    OutlineGPInfo.DepthStencilState.stencilTestEnable = VK_TRUE;
    OutlineGPInfo.DepthStencilState.back.compareOp = VK_COMPARE_OP_NOT_EQUAL;
    OutlineGPInfo.DepthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
    OutlineGPInfo.DepthStencilState.back.depthFailOp = VK_STENCIL_OP_KEEP;
    OutlineGPInfo.DepthStencilState.back.passOp = VK_STENCIL_OP_REPLACE;
    OutlineGPInfo.DepthStencilState.back.compareMask = 0xFF;
    OutlineGPInfo.DepthStencilState.back.writeMask = 0xFF;
    OutlineGPInfo.DepthStencilState.back.reference = 1;
    OutlineGPInfo.DepthStencilState.front = OutlineGPInfo.DepthStencilState.back;

    OutlineGPInfo.DepthStencilState.depthTestEnable = VK_FALSE;
    OutlineGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    OutlineGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    ctk::push(&VulkanState->GraphicsPipelines, "outline",
              vtk::create_graphics_pipeline(VulkanInstance->Device.Logical, ctk::at(&VulkanState->RenderPasses, "direct"), &OutlineGPInfo));

    // record_direct_render_pass(VulkanInstance, VulkanState, Scene);
    record_stencil_render_pass(VulkanInstance, VulkanState, Scene);
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
        update_uniform_data(VulkanInstance, Scene);
        render(VulkanInstance, VulkanState);
        Sleep(1);
    }
}
