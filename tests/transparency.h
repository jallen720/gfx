#pragma once

static void
transparency_test_create_state(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState)
{
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////

    // Blended GP
    vtk::graphics_pipeline *BlendedGP = ctk::push(&VulkanState->GraphicsPipelines, "blended");
    vtk::graphics_pipeline_info BlendedGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&BlendedGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "blended_vert"));
    ctk::push(&BlendedGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "blended_frag"));
    ctk::push(&BlendedGPInfo.DescriptorSetLayouts, ctk::at(&VulkanState->DescriptorSets, "entity")->Layout);
    ctk::push(&BlendedGPInfo.DescriptorSetLayouts, ctk::at(&VulkanState->DescriptorSets, "grass_texture")->Layout);
    ctk::push(&BlendedGPInfo.VertexInputs, { 0, 0, *ctk::at(&VulkanState->VertexAttributeIndexes, "position") });
    ctk::push(&BlendedGPInfo.VertexInputs, { 0, 1, *ctk::at(&VulkanState->VertexAttributeIndexes, "uv") });
    BlendedGPInfo.VertexLayout = &VulkanState->VertexLayout;
    ctk::push(&BlendedGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&BlendedGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    BlendedGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    BlendedGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    BlendedGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    *BlendedGP = vtk::create_graphics_pipeline(VulkanInstance->Device.Logical, ctk::at(&VulkanState->RenderPasses, "default"),
                                               &BlendedGPInfo);
}

static scene *
transparency_test_create_scene(assets *Assets, vulkan_state *VulkanState)
{
    return create_scene(Assets, VulkanState, "assets/scenes/transparency_test.ctkd");
}

static void
transparency_test_init(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState, scene *Scene)
{
    record_default_command_buffers(VulkanInstance, VulkanState, Scene);
}

static void
transparency_test_update(vulkan_instance *VulkanInstance, vulkan_state *VulkanState)
{
    render_default_render_pass(VulkanInstance, VulkanState);
}
