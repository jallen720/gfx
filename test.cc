#include <windows.h>
#include "gfx/gfx.h"

struct render_entity
{
    ctk::sarray<vtk::descriptor_set *, 4> DescriptorSets;
    vtk::graphics_pipeline *GraphicsPipeline;
    gfx::mesh *Mesh;
};

s32
main()
{
    gfx::input_state InputState = {};
    gfx::window *Window = gfx::CreateWindow_(&InputState);
    gfx::vulkan_instance *VulkanInstance = gfx::CreateVulkanInstance(Window);
    gfx::assets *Assets = gfx::CreateAssets(VulkanInstance);
    gfx::vulkan_state *VulkanState = gfx::CreateVulkanState(VulkanInstance, Assets);

    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;
    VkCommandPool *GraphicsCommandPool = &VulkanInstance->GraphicsCommandPool;
    vtk::render_pass *RenderPass = &VulkanInstance->RenderPass;
    ctk::sarray<VkFramebuffer, 4> *Framebuffers = &VulkanInstance->Framebuffers;
    ctk::sarray<VkCommandBuffer, 4> *CommandBuffers = &VulkanInstance->CommandBuffers;
    vtk::buffer *HostBuffer = &VulkanInstance->HostBuffer;
    vtk::buffer *DeviceBuffer = &VulkanInstance->DeviceBuffer;
    vtk::region *StagingRegion = &VulkanInstance->StagingRegion;

    ctk::smap<vtk::graphics_pipeline, 4> *GraphicsPipelines = &VulkanState->GraphicsPipelines;
    ctk::smap<vtk::descriptor_set, 4> *DescriptorSets = &VulkanState->DescriptorSets;
    ctk::smap<vtk::uniform_buffer, 4> *UniformBuffers = &VulkanState->UniformBuffers;

    ////////////////////////////////////////////////////////////
    /// Data
    ////////////////////////////////////////////////////////////

    // Meshes
    gfx::mesh Meshes[2] = {};
    gfx::mesh *QuadMesh = Meshes + 0;
    gfx::mesh *CubeMesh = Meshes + 1;
    u32 QuadIndexes[] = { 0, 1, 2, 0, 2, 3 };
    u32 CubeIndexes[] =
    {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
    };
    ctk::Push(&QuadMesh->Vertexes, { { 0.0f,  0.0f, 0.0f }, {}, { 0.0f, 1.0f } });
    ctk::Push(&QuadMesh->Vertexes, { { 1.0f,  0.0f, 0.0f }, {}, { 1.0f, 1.0f } });
    ctk::Push(&QuadMesh->Vertexes, { { 1.0f, -1.0f, 0.0f }, {}, { 1.0f, 0.0f } });
    ctk::Push(&QuadMesh->Vertexes, { { 0.0f, -1.0f, 0.0f }, {}, { 0.0f, 0.0f } });
    ctk::Push(&QuadMesh->Indexes, QuadIndexes, CTK_ARRAY_COUNT(QuadIndexes));
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f,  0.0f, 0.0f }, {}, { 0.0f, 1.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 1.0f,  0.0f, 0.0f }, {}, { 1.0f, 1.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 1.0f, -1.0f, 0.0f }, {}, { 1.0f, 0.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f, -1.0f, 0.0f }, {}, { 0.0f, 0.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f,  0.0f, 1.0f }, {}, { 0.0f, 1.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f,  0.0f, 0.0f }, {}, { 1.0f, 1.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f, -1.0f, 0.0f }, {}, { 1.0f, 0.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f, -1.0f, 1.0f }, {}, { 0.0f, 0.0f } });
    ctk::Push(&CubeMesh->Indexes, CubeIndexes, CTK_ARRAY_COUNT(CubeIndexes));
    for(u32 MeshIndex = 0; MeshIndex < CTK_ARRAY_COUNT(Meshes); ++MeshIndex)
    {
        gfx::mesh *Mesh = Meshes + MeshIndex;
        u32 VertexByteCount = ctk::ByteCount(&Mesh->Vertexes);
        u32 IndexByteCount = ctk::ByteCount(&Mesh->Indexes);
        Mesh->VertexRegion = vtk::AllocateRegion(DeviceBuffer, VertexByteCount);
        Mesh->IndexRegion = vtk::AllocateRegion(DeviceBuffer, IndexByteCount);
        vtk::WriteToDeviceRegion(Device, *GraphicsCommandPool, StagingRegion, &Mesh->VertexRegion,
                                 Mesh->Vertexes.Data, VertexByteCount, 0);
        vtk::WriteToDeviceRegion(Device, *GraphicsCommandPool, StagingRegion, &Mesh->IndexRegion,
                                 Mesh->Indexes.Data, IndexByteCount, 0);
    }

    ////////////////////////////////////////////////////////////
    /// Scene
    ////////////////////////////////////////////////////////////
    ctk::sarray<render_entity, 4> RenderEntities = {};
    render_entity *QuadEntity = ctk::Push(&RenderEntities);
    ctk::Push(&QuadEntity->DescriptorSets, At(DescriptorSets, "entity"));
    ctk::Push(&QuadEntity->DescriptorSets, At(DescriptorSets, "grass_texture"));
    QuadEntity->GraphicsPipeline = ctk::At(GraphicsPipelines, "default");
    QuadEntity->Mesh = QuadMesh;

    render_entity *CubeEntity = ctk::Push(&RenderEntities);
    ctk::Push(&CubeEntity->DescriptorSets, At(DescriptorSets, "entity"));
    ctk::Push(&CubeEntity->DescriptorSets, At(DescriptorSets, "dirt_texture"));
    CubeEntity->GraphicsPipeline = ctk::At(GraphicsPipelines, "default");
    CubeEntity->Mesh = CubeMesh;

    ////////////////////////////////////////////////////////////
    /// Record render pass.
    ////////////////////////////////////////////////////////////
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
        VkCommandBuffer CommandBuffer = *At(CommandBuffers, FrameIndex);
        vtk::ValidateVkResult(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                              "vkBeginCommandBuffer", "failed to begin recording command buffer");
        VkRenderPassBeginInfo RenderPassBeginInfo = {};
        RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassBeginInfo.renderPass = RenderPass->Handle;
        RenderPassBeginInfo.framebuffer = *At(Framebuffers, FrameIndex);
        RenderPassBeginInfo.renderArea = RenderArea;
        RenderPassBeginInfo.clearValueCount = RenderPass->ClearValues.Count;
        RenderPassBeginInfo.pClearValues = RenderPass->ClearValues.Data;

        // Begin
        vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Render Commands
        for(u32 EntityIndex = 0; EntityIndex < RenderEntities.Count; ++EntityIndex)
        {
            render_entity *RenderEntity = RenderEntities + EntityIndex;
            gfx::mesh *Mesh = RenderEntity->Mesh;

            // Graphics Pipeline
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RenderEntity->GraphicsPipeline->Handle);

            // Descriptor Sets
            vtk::BindDescriptorSets(CommandBuffer, RenderEntity->GraphicsPipeline->Layout,
                                    RenderEntity->DescriptorSets.Data, RenderEntity->DescriptorSets.Count,
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

    ////////////////////////////////////////////////////////////
    /// Main Loop
    ////////////////////////////////////////////////////////////
    b32 Close = false;
    glm::vec3 CameraPosition = { 0.0f, 0.0f, -1.0f };
    glm::vec3 CameraRotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 EntityPositions[2] =
    {
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 3.0f },
    };
    while(!glfwWindowShouldClose(Window->Handle) && !Close)
    {
        ////////////////////////////////////////////////////////////
        /// Input
        ////////////////////////////////////////////////////////////
        glfwPollEvents();
        if(InputState.KeyDown[GLFW_KEY_ESCAPE])
        {
            Close = true;
        }

        // Mouse Delta
        ctk::vec2 PreviousMousePosition = InputState.MousePosition;
        f64 CurrentMouseX = 0.0;
        f64 CurrentMouseY = 0.0;
        glfwGetCursorPos(Window->Handle, &CurrentMouseX, &CurrentMouseY);
        InputState.MousePosition = { CurrentMouseX, CurrentMouseY };

        // Calculate delta if previous position was not unset.
        if(PreviousMousePosition != gfx::UNSET_MOUSE_POSITION)
        {
            InputState.MouseDelta = InputState.MousePosition - PreviousMousePosition;
        }

        ////////////////////////////////////////////////////////////
        /// Camera Controls
        ////////////////////////////////////////////////////////////

        // Rotation
        if(InputState.MouseButtonDown[GLFW_MOUSE_BUTTON_2])
        {
            static const f32 SENS = 0.4f;
            CameraRotation.x += InputState.MouseDelta.Y * SENS;
            CameraRotation.y -= InputState.MouseDelta.X * SENS;
            CameraRotation.x = ctk::Clamp(CameraRotation.x, -80.0f, 80.0f);
        }

        // Translation
        glm::vec3 Translation = {};
        f32 Modifier = InputState.KeyDown[GLFW_KEY_LEFT_SHIFT] ? 4 :
                       InputState.KeyDown[GLFW_KEY_LEFT_CONTROL] ? 1 :
                       2;
        if(InputState.KeyDown[GLFW_KEY_W]) Translation.z += 0.01f * Modifier;
        if(InputState.KeyDown[GLFW_KEY_S]) Translation.z -= 0.01f * Modifier;
        if(InputState.KeyDown[GLFW_KEY_D]) Translation.x += 0.01f * Modifier;
        if(InputState.KeyDown[GLFW_KEY_A]) Translation.x -= 0.01f * Modifier;
        if(InputState.KeyDown[GLFW_KEY_E]) Translation.y -= 0.01f * Modifier;
        if(InputState.KeyDown[GLFW_KEY_Q]) Translation.y += 0.01f * Modifier;

        glm::mat4 CameraWorldMatrix(1.0f);
        CameraWorldMatrix = glm::rotate(CameraWorldMatrix, glm::radians(CameraRotation.x), { 1.0f, 0.0f, 0.0f });
        CameraWorldMatrix = glm::rotate(CameraWorldMatrix, glm::radians(CameraRotation.y), { 0.0f, 1.0f, 0.0f });
        CameraWorldMatrix = glm::rotate(CameraWorldMatrix, glm::radians(CameraRotation.z), { 0.0f, 0.0f, 1.0f });
        CameraWorldMatrix = glm::translate(CameraWorldMatrix, { CameraPosition.x, CameraPosition.y, CameraPosition.z });

        glm::vec3 Right = {};
        Right.x = CameraWorldMatrix[0][0];
        Right.y = CameraWorldMatrix[1][0];
        Right.z = CameraWorldMatrix[2][0];

        glm::vec3 Up = {};
        Up.x = CameraWorldMatrix[0][1];
        Up.y = CameraWorldMatrix[1][1];
        Up.z = CameraWorldMatrix[2][1];

        glm::vec3 Forward = {};
        Forward.x = CameraWorldMatrix[0][2];
        Forward.y = CameraWorldMatrix[1][2];
        Forward.z = CameraWorldMatrix[2][2];

        glm::vec3 NewPosition = CameraPosition;
        NewPosition = NewPosition + (Right * Translation.x);
        NewPosition = NewPosition + (Up * Translation.y);
        CameraPosition = NewPosition + (Forward * Translation.z);

        ////////////////////////////////////////////////////////////
        /// Update Uniform Data
        ////////////////////////////////////////////////////////////
        gfx::entity_ubo EntityUBOs[2] = {};

        // View Matrix
        glm::mat4 CameraMatrix(1.0f);
        CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraRotation.x), { 1.0f, 0.0f, 0.0f });
        CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraRotation.y), { 0.0f, 1.0f, 0.0f });
        CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraRotation.z), { 0.0f, 0.0f, 1.0f });
        CameraMatrix = glm::translate(CameraMatrix, CameraPosition);
        glm::vec3 CameraForward = { CameraMatrix[0][2], CameraMatrix[1][2], CameraMatrix[2][2] };
        glm::mat4 ViewMatrix = glm::lookAt(CameraPosition, CameraPosition + CameraForward, { 0.0f, -1.0f, 0.0f });

        // Projection Matrix
        f32 Aspect = Swapchain->Extent.width / (f32)Swapchain->Extent.height;
        glm::mat4 ProjectionMatrix = glm::perspective(glm::radians(90.0f), Aspect, 0.1f, 1000.0f);
        ProjectionMatrix[1][1] *= -1; // Flip y value for scale (glm is designed for OpenGL).

        // Entity Model Matrixes
        for(u32 EntityIndex = 0; EntityIndex < 2; ++EntityIndex)
        {
            glm::mat4 ModelMatrix(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, EntityPositions[EntityIndex]);
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(0.0f), { 1.0f, 0.0f, 0.0f });
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(0.0f), { 0.0f, 1.0f, 0.0f });
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(0.0f), { 0.0f, 0.0f, 1.0f });
            ModelMatrix = glm::scale(ModelMatrix, { 1.0f, 1.0f, 1.0f });
            EntityUBOs[EntityIndex].ModelMatrix = ModelMatrix;
            EntityUBOs[EntityIndex].MVPMatrix = ProjectionMatrix * ViewMatrix * ModelMatrix;
        }
        vtk::WriteToHostRegion(Device->Logical, ctk::At(UniformBuffers, "entity")->Regions + FrameState->CurrentFrameIndex,
                               EntityUBOs, sizeof(EntityUBOs), 0);

        ////////////////////////////////////////////////////////////
        /// Rendering
        ////////////////////////////////////////////////////////////
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
        VkCommandBuffer QueueSubmitCommandBuffers[] = { *At(CommandBuffers, SwapchainImageIndex) };

        VkSubmitInfo SubmitInfos[1] = {};
        SubmitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfos[0].waitSemaphoreCount = CTK_ARRAY_COUNT(QueueSubmitWaitSemaphores);
        SubmitInfos[0].pWaitSemaphores = QueueSubmitWaitSemaphores;
        SubmitInfos[0].pWaitDstStageMask = QueueSubmitWaitStages;
        SubmitInfos[0].commandBufferCount = CTK_ARRAY_COUNT(QueueSubmitCommandBuffers);
        SubmitInfos[0].pCommandBuffers = QueueSubmitCommandBuffers;
        SubmitInfos[0].signalSemaphoreCount = CTK_ARRAY_COUNT(QueueSubmitSignalSemaphores);
        SubmitInfos[0].pSignalSemaphores = QueueSubmitSignalSemaphores;
        {
            // Submit render pass commands to graphics queue for rendering.
            // Signal current frame's in flight flence when commands have finished executing.
            VkResult Result = vkQueueSubmit(Device->GraphicsQueue, CTK_ARRAY_COUNT(SubmitInfos), SubmitInfos, CurrentFrame->InFlightFence);
            vtk::ValidateVkResult(Result, "vkQueueSubmit", "failed to submit command buffer to graphics queue");
        }

        ////////////////////////////////////////////////////////////
        /// Presentation
        ////////////////////////////////////////////////////////////

        // These are parallel; provide 1:1 index per swapchain.
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
        {
            // Submit Swapchains to present queue for presentation once rendering is complete.
            VkResult Result = vkQueuePresentKHR(Device->PresentQueue, &PresentInfo);
            vtk::ValidateVkResult(Result, "vkQueuePresentKHR", "failed to queue image for presentation");
        }

        // Cycle frame.
        FrameState->CurrentFrameIndex = (FrameState->CurrentFrameIndex + 1) % FrameState->Frames.Count;

        Sleep(1);
    }
}
