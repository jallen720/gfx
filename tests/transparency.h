#pragma once

#include "ctk/math.h"

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
    BlendedGPInfo.VertexLayout = &VulkanState->VertexLayout;
    ctk::push(&BlendedGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&BlendedGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&BlendedGPInfo.ColorBlendAttachmentStates,
              {
                  VK_TRUE,
                  VK_BLEND_FACTOR_SRC_ALPHA,
                  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                  VK_BLEND_OP_ADD,
                  VK_BLEND_FACTOR_ONE,
                  VK_BLEND_FACTOR_ZERO,
                  VK_BLEND_OP_ADD,
                  VTK_COLOR_COMPONENT_RGBA,
              });
    BlendedGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    BlendedGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    BlendedGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    *BlendedGP = vtk::create_graphics_pipeline(VulkanInstance->Device.Logical, ctk::at(&VulkanState->RenderPasses, "default"),
                                               &BlendedGPInfo);
}

static scene *
transparency_test_create_scene(assets *Assets, vulkan_state *VulkanState)
{
    scene *Scene = create_scene(Assets, VulkanState, "assets/scenes/transparency_test.ctkd");
    CTK_REPEAT(100)
    {
        char Name[16] = {};
        sprintf(Name, "cube_%u", RepeatIndex);
        entity *Cube = push_entity(Scene, Name);
        Cube->Transform.Position = { (f32)ctk::random_range(0, 20), -(f32)ctk::random_range(0, 20), (f32)ctk::random_range(0, 20) };
        Cube->Transform.Scale = { 1, 1, 1 };
        ctk::push(&Cube->DescriptorSets, ctk::at(&VulkanState->DescriptorSets, "entity"));
        Cube->GraphicsPipeline = ctk::at(&VulkanState->GraphicsPipelines, "blended");
        Cube->Mesh = ctk::at(&Assets->Meshes, "cube");
    }
    return Scene;
}

static void
transparency_test_init(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState, scene *Scene)
{
    record_default_command_buffers(VulkanInstance, VulkanState, Scene);
}

static void
transparency_test_update(vulkan_instance *VulkanInstance, vulkan_state *VulkanState, u32 SwapchainImageIndex)
{
    render_default_render_pass(VulkanInstance, VulkanState, SwapchainImageIndex);
}
