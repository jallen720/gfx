#pragma once

static void
stencil_test_create_state(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::render_pass *DefaultRP = ctk::at(&VulkanState->RenderPasses, "default");
    vtk::graphics_pipeline *StencilRenderGP = ctk::push(&VulkanState->GraphicsPipelines, "stencil_render");
    vtk::graphics_pipeline *OutlineGP = ctk::push(&VulkanState->GraphicsPipelines, "outline");

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////

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

    *StencilRenderGP = vtk::create_graphics_pipeline(VulkanInstance->Device.Logical, DefaultRP, &StencilRenderGPInfo);

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

    *OutlineGP = vtk::create_graphics_pipeline(VulkanInstance->Device.Logical, DefaultRP, &OutlineGPInfo);
}

static scene *
stencil_test_create_scene(assets *Assets, vulkan_state *VulkanState)
{
    return create_scene(Assets, VulkanState, "assets/scenes/default.ctkd");
}

static void
stencil_test_init(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState, scene *Scene)
{
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::render_pass *DefaultRP = ctk::at(&VulkanState->RenderPasses, "default");
    vtk::graphics_pipeline *StencilRenderGP = ctk::at(&VulkanState->GraphicsPipelines, "stencil_render");
    vtk::graphics_pipeline *OutlineGP = ctk::at(&VulkanState->GraphicsPipelines, "outline");

    ////////////////////////////////////////////////////////////
    /// Record Command Buffers
    ////////////////////////////////////////////////////////////
    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = Swapchain->Extent;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = 0;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

    for(u32 SwapchainImageIndex = 0; SwapchainImageIndex < Swapchain->Images.Count; ++SwapchainImageIndex)
    {
        VkCommandBuffer CommandBuffer = *ctk::at(&DefaultRP->CommandBuffers, SwapchainImageIndex);
        vtk::validate_vk_result(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                                "vkBeginCommandBuffer", "failed to begin recording command buffer");
        VkRenderPassBeginInfo RenderPassBeginInfo = {};
        RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassBeginInfo.renderPass = DefaultRP->Handle;
        RenderPassBeginInfo.framebuffer = *ctk::at(&DefaultRP->Framebuffers, SwapchainImageIndex);
        RenderPassBeginInfo.renderArea = RenderArea;
        RenderPassBeginInfo.clearValueCount = DefaultRP->ClearValues.Count;
        RenderPassBeginInfo.pClearValues = DefaultRP->ClearValues.Data;

        // Begin
        vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        ////////////////////////////////////////////////////////////
        /// Render Commands
        ////////////////////////////////////////////////////////////
        for(u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex)
        {
            entity *Entity = Scene->Entities.Values + EntityIndex;
            mesh *Mesh = Entity->Mesh;

            // Vertex/Index Buffers
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
            vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);

            ////////////////////////////////////////////////////////////
            /// Stencil Render
            ////////////////////////////////////////////////////////////

            // Graphics Pipeline
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, StencilRenderGP->Handle);

            // Descriptor Sets
            vtk::bind_descriptor_sets(CommandBuffer, StencilRenderGP->Layout,
                                      Entity->DescriptorSets.Data, Entity->DescriptorSets.Count,
                                      SwapchainImageIndex, EntityIndex);

            // Draw
            vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);

            ////////////////////////////////////////////////////////////
            /// Outline
            ////////////////////////////////////////////////////////////

            // Graphics Pipeline
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, OutlineGP->Handle);

            // Descriptor Sets
            vtk::descriptor_set* DescriptorSet = ctk::at(&VulkanState->DescriptorSets, "entity");
            vtk::bind_descriptor_sets(CommandBuffer, OutlineGP->Layout,
                                      &DescriptorSet, 1,
                                      SwapchainImageIndex, EntityIndex);

            // Draw
            vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
        }

        // End
        vkCmdEndRenderPass(CommandBuffer);
        vtk::validate_vk_result(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
    }
}

static void
stencil_test_update(vulkan_instance *VulkanInstance, vulkan_state *VulkanState)
{
    render_default_render_pass(VulkanInstance, VulkanState);
}
