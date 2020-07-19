#include <windows.h>
#include "gfx/graphics.h"
#include "ctk/data.h"

////////////////////////////////////////////////////////////
/// Data
////////////////////////////////////////////////////////////
struct transform
{
    ctk::vec3<f32> Position;
    ctk::vec3<f32> Rotation;
    ctk::vec3<f32> Scale;
};

struct camera
{
    transform Transform;
    f32 FieldOfView;
};

struct entity
{
    transform Transform;
    ctk::sarray<vtk::descriptor_set *, 4> DescriptorSets;
    vtk::graphics_pipeline *GraphicsPipeline;
    gfx_mesh *Mesh;
};

struct scene
{
    camera Camera;
    ctk::smap<entity, 64> Entities;
    ctk::sarray<gfx_entity_ubo, 64> EntityUBOs;
    vtk::uniform_buffer *EntityUniformBuffer;
};

////////////////////////////////////////////////////////////
/// Internal
////////////////////////////////////////////////////////////
static ctk::vec3<f32>
load_vec3(ctk::data *Data)
{
    return { ctk::F32(Data, 0u), ctk::F32(Data, 1u), ctk::F32(Data, 2u) };
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
        Transform.Position = load_vec3(At(Data, "position"));
        Transform.Rotation = load_vec3(At(Data, "rotation"));
        Transform.Scale = load_vec3(At(Data, "scale"));
    }
    return Transform;
}

static scene *
create_scene(gfx_vulkan_state *VulkanState, gfx_assets *Assets, cstr Path)
{
    scene *Scene = ctk::Alloc<scene>();
    *Scene = {};

    ctk::data SceneData = ctk::LoadData(Path);

    // Camera
    ctk::data *CameraData = ctk::At(&SceneData, "camera");
    Scene->Camera.Transform = load_transform(ctk::At(CameraData, "transform"));
    Scene->Camera.FieldOfView = ctk::F32(CameraData, "field_of_view");

    // Entities
    ctk::data *EntityMap = ctk::At(&SceneData, "entities");
    for(u32 EntityIndex = 0; EntityIndex < EntityMap->Children.Count; ++EntityIndex)
    {
        ctk::data *EntityData = ctk::At(EntityMap, EntityIndex);
        entity *Entity = ctk::Push(&Scene->Entities, EntityData->Key.Data);
        ctk::Push(&Scene->EntityUBOs);
        Entity->Transform = load_transform(ctk::At(EntityData, "transform"));
        ctk::data *DescriptorSetArray = ctk::At(EntityData, "descriptor_sets");
        for(u32 DescriptorSetIndex = 0; DescriptorSetIndex < DescriptorSetArray->Children.Count; ++DescriptorSetIndex)
        {
            ctk::Push(&Entity->DescriptorSets, At(&VulkanState->DescriptorSets, ctk::CStr(DescriptorSetArray, DescriptorSetIndex)));
        }
        Entity->GraphicsPipeline = ctk::At(&VulkanState->GraphicsPipelines, ctk::CStr(EntityData, "graphics_pipeline"));
        Entity->Mesh = ctk::At(&Assets->Meshes, ctk::CStr(EntityData, "mesh"));
    }
    Scene->EntityUniformBuffer = ctk::At(&VulkanState->UniformBuffers, "entity");

    // Cleanup
    ctk::Free(&SceneData);

    return Scene;
}

static void
record_render_pass(gfx_vulkan_instance *VulkanInstance, scene *Scene)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;
    vtk::render_pass *RenderPass = &VulkanInstance->RenderPass;

    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = Swapchain->Extent;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = 0;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

    for(u32 FrameIndex = 0; FrameIndex < FrameState->Frames.Count; ++FrameIndex)
    {
        VkCommandBuffer CommandBuffer = *At(&RenderPass->CommandBuffers, FrameIndex);
        vtk::ValidateVkResult(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                              "vkBeginCommandBuffer", "failed to begin recording command buffer");
        VkRenderPassBeginInfo RenderPassBeginInfo = {};
        RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassBeginInfo.renderPass = RenderPass->Handle;
        RenderPassBeginInfo.framebuffer = *At(&RenderPass->Framebuffers, FrameIndex);
        RenderPassBeginInfo.renderArea = RenderArea;
        RenderPassBeginInfo.clearValueCount = RenderPass->ClearValues.Count;
        RenderPassBeginInfo.pClearValues = RenderPass->ClearValues.Data;

        // Begin
        vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Render Commands
        for(u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex)
        {
            entity *Entity = Scene->Entities.Values + EntityIndex;
            gfx_mesh *Mesh = Entity->Mesh;

            // Graphics Pipeline
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Entity->GraphicsPipeline->Handle);

            // Descriptor Sets
            vtk::BindDescriptorSets(CommandBuffer, Entity->GraphicsPipeline->Layout,
                                    Entity->DescriptorSets.Data, Entity->DescriptorSets.Count,
                                    FrameIndex, EntityIndex);

            // Vertex/Index Buffers
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
            vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);

            // Draw
            vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
        }

        // End
        vkCmdEndRenderPass(CommandBuffer);
        vtk::ValidateVkResult(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
    }
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
update_input_state(gfx_input_state *InputState, GLFWwindow *Window)
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
camera_controls(transform *CameraTransform, gfx_input_state *InputState)
{
    // Rotation
    if(InputState->MouseButtonDown[GLFW_MOUSE_BUTTON_2])
    {
        static const f32 SENS = 0.4f;
        CameraTransform->Rotation.X += InputState->MouseDelta.Y * SENS;
        CameraTransform->Rotation.Y -= InputState->MouseDelta.X * SENS;
        CameraTransform->Rotation.X = ctk::Clamp(CameraTransform->Rotation.X, -80.0f, 80.0f);
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

static void
update_uniform_data(scene *Scene, gfx_vulkan_instance *VulkanInstance)
{
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    transform *CameraTransform = &Scene->Camera.Transform;

    // View Matrix
    glm::vec3 CameraPosition = { CameraTransform->Position.X, CameraTransform->Position.Y, CameraTransform->Position.Z };
    glm::mat4 CameraMatrix(1.0f);
    CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraTransform->Rotation.X), { 1.0f, 0.0f, 0.0f });
    CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraTransform->Rotation.Y), { 0.0f, 1.0f, 0.0f });
    CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraTransform->Rotation.Z), { 0.0f, 0.0f, 1.0f });
    CameraMatrix = glm::translate(CameraMatrix, CameraPosition);
    glm::vec3 CameraForward = { CameraMatrix[0][2], CameraMatrix[1][2], CameraMatrix[2][2] };
    glm::mat4 ViewMatrix = glm::lookAt(CameraPosition, CameraPosition + CameraForward, { 0.0f, -1.0f, 0.0f });

    // Projection Matrix
    f32 Aspect = Swapchain->Extent.width / (f32)Swapchain->Extent.height;
    glm::mat4 ProjectionMatrix = glm::perspective(glm::radians(Scene->Camera.FieldOfView), Aspect, 0.1f, 1000.0f);
    ProjectionMatrix[1][1] *= -1; // Flip y value for scale (glm is designed for OpenGL).

    // Entity Model Matrixes
    for(u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex)
    {
        transform *EntityTransform = &Scene->Entities.Values[EntityIndex].Transform;
        glm::mat4 ModelMatrix(1.0f);
        ModelMatrix = glm::translate(ModelMatrix, { EntityTransform->Position.X, EntityTransform->Position.Y, EntityTransform->Position.Z });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(EntityTransform->Rotation.X), { 1.0f, 0.0f, 0.0f });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(EntityTransform->Rotation.Y), { 0.0f, 1.0f, 0.0f });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(EntityTransform->Rotation.Z), { 0.0f, 0.0f, 1.0f });
        ModelMatrix = glm::scale(ModelMatrix, { EntityTransform->Scale.X, EntityTransform->Scale.Y, EntityTransform->Scale.Z });
        Scene->EntityUBOs[EntityIndex].ModelMatrix = ModelMatrix;
        Scene->EntityUBOs[EntityIndex].MVPMatrix = ProjectionMatrix * ViewMatrix * ModelMatrix;
    }

    // Write all entity ubos to current frame's entity uniform buffer region.
    vtk::region *EntityUniformBufferRegion = Scene->EntityUniformBuffer->Regions + VulkanInstance->FrameState.CurrentFrameIndex;
    vtk::WriteToHostRegion(VulkanInstance->Device.Logical, EntityUniformBufferRegion,
                           Scene->EntityUBOs.Data, ctk::ByteCount(&Scene->EntityUBOs), 0);
}

static void
render(gfx_vulkan_instance *VulkanInstance)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;
    vtk::render_pass *RenderPass = &VulkanInstance->RenderPass;

    vtk::frame *CurrentFrame = FrameState->Frames + FrameState->CurrentFrameIndex;

    // Wait on current frame's fence if still unsignaled.
    vkWaitForFences(Device->Logical, 1, &CurrentFrame->InFlightFence, VK_TRUE, UINT64_MAX);

    // Aquire next swapchain image index, using a semaphore to signal when image is available for rendering.
    u32 SwapchainImageIndex = VTK_UNSET_INDEX;
    {
        VkResult Result = vkAcquireNextImageKHR(Device->Logical, Swapchain->Handle, UINT64_MAX, CurrentFrame->ImageAquiredSemaphore,
                                                VK_NULL_HANDLE, &SwapchainImageIndex);
        vtk::ValidateVkResult(Result, "vkAcquireNextImageKHR", "failed to aquire next swapchain image");
    }

    // Wait on swapchain images previously associated frame fence before rendering.
    VkFence *PreviousFrameInFlightFence = FrameState->PreviousFrameInFlightFences + SwapchainImageIndex;
    if(*PreviousFrameInFlightFence != VK_NULL_HANDLE)
    {
        vkWaitForFences(Device->Logical, 1, PreviousFrameInFlightFence, VK_TRUE, UINT64_MAX);
    }
    vkResetFences(Device->Logical, 1, &CurrentFrame->InFlightFence);
    *PreviousFrameInFlightFence = CurrentFrame->InFlightFence;

    ////////////////////////////////////////////////////////////
    /// Submit Render Pass Command Buffer
    ////////////////////////////////////////////////////////////
    VkSemaphore QueueSubmitWaitSemaphores[] = { CurrentFrame->ImageAquiredSemaphore };
    VkPipelineStageFlags QueueSubmitWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore QueueSubmitSignalSemaphores[] = { CurrentFrame->RenderFinishedSemaphore };
    VkCommandBuffer QueueSubmitCommandBuffers[] = { *At(&VulkanInstance->RenderPass.CommandBuffers, SwapchainImageIndex) };

    VkSubmitInfo SubmitInfos[1] = {};
    SubmitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfos[0].waitSemaphoreCount = CTK_ARRAY_COUNT(QueueSubmitWaitSemaphores);
    SubmitInfos[0].pWaitSemaphores = QueueSubmitWaitSemaphores;
    SubmitInfos[0].pWaitDstStageMask = QueueSubmitWaitStages;
    SubmitInfos[0].commandBufferCount = CTK_ARRAY_COUNT(QueueSubmitCommandBuffers);
    SubmitInfos[0].pCommandBuffers = QueueSubmitCommandBuffers;
    SubmitInfos[0].signalSemaphoreCount = CTK_ARRAY_COUNT(QueueSubmitSignalSemaphores);
    SubmitInfos[0].pSignalSemaphores = QueueSubmitSignalSemaphores;

    // Submit render pass commands to graphics queue for rendering.
    // Signal current frame's in flight flence when commands have finished executing.
    vtk::ValidateVkResult(vkQueueSubmit(Device->GraphicsQueue, CTK_ARRAY_COUNT(SubmitInfos), SubmitInfos, CurrentFrame->InFlightFence),
                          "vkQueueSubmit", "failed to submit command buffer to graphics queue");

    ////////////////////////////////////////////////////////////
    /// Presentation
    ////////////////////////////////////////////////////////////

    // Provide 1:1 index per swapchain.
    VkSwapchainKHR Swapchains[] = { Swapchain->Handle };
    u32 SwapchainImageIndexes[] = { SwapchainImageIndex };

    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = CTK_ARRAY_COUNT(QueueSubmitSignalSemaphores);
    PresentInfo.pWaitSemaphores = QueueSubmitSignalSemaphores;
    PresentInfo.swapchainCount = CTK_ARRAY_COUNT(Swapchains);
    PresentInfo.pSwapchains = Swapchains;
    PresentInfo.pImageIndices = SwapchainImageIndexes;
    PresentInfo.pResults = NULL;

    // Submit Swapchains to present queue for presentation once rendering is complete.
    vtk::ValidateVkResult(vkQueuePresentKHR(Device->PresentQueue, &PresentInfo), "vkQueuePresentKHR",
                          "failed to queue image for presentation");

    // Cycle frame.
    FrameState->CurrentFrameIndex = (FrameState->CurrentFrameIndex + 1) % FrameState->Frames.Count;
}

////////////////////////////////////////////////////////////
/// Main
////////////////////////////////////////////////////////////
s32
main()
{
    gfx_input_state InputState = {};
    gfx_window *Window = gfx_create_window(&InputState);
    gfx_vulkan_instance *VulkanInstance = gfx_create_vulkan_instance(Window);
    gfx_assets *Assets = gfx_create_assets(VulkanInstance);
    gfx_vulkan_state *VulkanState = gfx_create_vulkan_state(VulkanInstance, Assets);

    scene *Scene = create_scene(VulkanState, Assets, "assets/scenes/test_scene.ctkd");
    record_render_pass(VulkanInstance, Scene);
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
        update_uniform_data(Scene, VulkanInstance);
        render(VulkanInstance);
        Sleep(1);
    }
}
