#pragma once

#include "ctk/math.h"

static void
transparency_test_create_state(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    vtk::image *DepthImage = ctk::at(&VulkanState->Images, "depth");

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass_info TestRenderPassInfo = {};

    u32 DepthAttachmentIndex = TestRenderPassInfo.Attachments.Count;
    vtk::attachment *DepthAttachment = ctk::push(&TestRenderPassInfo.Attachments);
    DepthAttachment->Description.format = DepthImage->Format;
    DepthAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    DepthAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    DepthAttachment->ClearValue = { 1.0f, 0 };

    // u32 ColorAttachmentIndex = TestRenderPassInfo.Attachments.Count;
    // vtk::attachment *ColorAttachment = ctk::push(&TestRenderPassInfo.Attachments);
    // ColorAttachment->Description.format = Swapchain->ImageFormat;
    // ColorAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    // ColorAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // ColorAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // ColorAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    // ColorAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // ColorAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // ColorAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // ColorAttachment->ClearValue = { 0.04f, 0.04f, 0.04f, 1.0f };

    u32 PresentAttachmentIndex = TestRenderPassInfo.Attachments.Count;
    vtk::attachment *ColorAttachment = ctk::push(&TestRenderPassInfo.Attachments);
    ColorAttachment->Description.format = Swapchain->ImageFormat;
    ColorAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ColorAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    ColorAttachment->ClearValue = { 0.04f, 0.04f, 0.04f, 1.0f };

    // Subpasses
    vtk::subpass *InitialSubpass = ctk::push(&TestRenderPassInfo.Subpasses);
    ctk::set(&InitialSubpass->DepthAttachmentReference, { DepthAttachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
    ctk::push(&InitialSubpass->ColorAttachmentReferences, { PresentAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    // vtk::subpass *InputSubpass = ctk::push(&TestRenderPassInfo.Subpasses);

    // Subpass Dependencies
    VkSubpassDependency *DefaultSubpassDependency = ctk::push(&TestRenderPassInfo.SubpassDependencies);
    DefaultSubpassDependency->srcSubpass = VK_SUBPASS_EXTERNAL;
    DefaultSubpassDependency->dstSubpass = 0;
    DefaultSubpassDependency->srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    DefaultSubpassDependency->dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    DefaultSubpassDependency->srcAccessMask = 0;
    DefaultSubpassDependency->dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Framebuffer Infos
    for(u32 FramebufferIndex = 0; FramebufferIndex < Swapchain->Images.Count; ++FramebufferIndex)
    {
        vtk::framebuffer_info *FramebufferInfo = ctk::push(&TestRenderPassInfo.FramebufferInfos);
        ctk::push(&FramebufferInfo->Attachments, DepthImage->View);
        // ctk::push(&FramebufferInfo->Attachments, ColorImage->View);
        ctk::push(&FramebufferInfo->Attachments, Swapchain->Images[FramebufferIndex].View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    vtk::render_pass *TestRP = ctk::push(&VulkanState->RenderPasses, "test");
    *TestRP = vtk::create_render_pass(Device->Logical, VulkanInstance->GraphicsCommandPool, &TestRenderPassInfo);

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
#if 1
                  VK_BLEND_FACTOR_SRC_ALPHA,
                  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                  VK_BLEND_OP_ADD,
                  VK_BLEND_FACTOR_ONE,
                  VK_BLEND_FACTOR_ZERO,
                  VK_BLEND_OP_ADD,
#else
                  // Underblend
                  VK_BLEND_FACTOR_DST_ALPHA,
                  VK_BLEND_FACTOR_ONE,
                  VK_BLEND_OP_ADD,
                  VK_BLEND_FACTOR_ZERO,
                  VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                  VK_BLEND_OP_ADD,
#endif
                  VTK_COLOR_COMPONENT_RGBA,
              });
    BlendedGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    BlendedGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    BlendedGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    // BlendedGPInfo.RasterizationState.cullMode = VK_CULL_MODE_NONE;
    *BlendedGP = vtk::create_graphics_pipeline(Device->Logical, TestRP, &BlendedGPInfo);

//     // Present GP


//     vtk::graphics_pipeline *PresentGP = ctk::push(&VulkanState->GraphicsPipelines, "present");
//     vtk::graphics_pipeline_info PresentGPInfo = vtk::default_graphics_pipeline_info();
//     ctk::push(&PresentGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "present_vert"));
//     ctk::push(&PresentGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "present_frag"));
//     ctk::push(&PresentGPInfo.DescriptorSetLayouts, ctk::at(&VulkanState->DescriptorSets, "entity")->Layout);
//     ctk::push(&PresentGPInfo.DescriptorSetLayouts, ctk::at(&VulkanState->DescriptorSets, "grass_texture")->Layout);
//     ctk::push(&PresentGPInfo.VertexInputs, { 0, 0, *ctk::at(&VulkanState->VertexAttributeIndexes, "position") });
//     PresentGPInfo.VertexLayout = &VulkanState->VertexLayout;
//     ctk::push(&PresentGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
//     ctk::push(&PresentGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
//     ctk::push(&PresentGPInfo.ColorBlendAttachmentStates,
//               {
//                   VK_TRUE,
// #if 1
//                   VK_BLEND_FACTOR_SRC_ALPHA,
//                   VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
//                   VK_BLEND_OP_ADD,
//                   VK_BLEND_FACTOR_ONE,
//                   VK_BLEND_FACTOR_ZERO,
//                   VK_BLEND_OP_ADD,
// #else
//                   // Underblend
//                   VK_BLEND_FACTOR_DST_ALPHA,
//                   VK_BLEND_FACTOR_ONE,
//                   VK_BLEND_OP_ADD,
//                   VK_BLEND_FACTOR_ZERO,
//                   VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
//                   VK_BLEND_OP_ADD,
// #endif
//                   VTK_COLOR_COMPONENT_RGBA,
//               });
//     PresentGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
//     PresentGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
//     PresentGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
//     // PresentGPInfo.RasterizationState.cullMode = VK_CULL_MODE_NONE;
//     *PresentGP = vtk::create_graphics_pipeline(Device->Logical, TestRP, &PresentGPInfo);
}

static scene *
transparency_test_create_scene(assets *Assets, vulkan_state *VulkanState)
{
    scene *Scene = create_scene(Assets, VulkanState, "assets/scenes/transparency_test.ctkd");
    CTK_REPEAT(4)
    {
        char Name[16] = {};
        sprintf(Name, "cube_%u", RepeatIndex);
        entity *Cube = push_entity(Scene, Name);
        Cube->Transform.Position = { 0, 0, 1.5f * RepeatIndex };
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
    record_default_command_buffers(VulkanInstance, VulkanState, ctk::at(&VulkanState->RenderPasses, "test"), Scene);
}

static void
transparency_test_update(vulkan_instance *VulkanInstance, vulkan_state *VulkanState, u32 SwapchainImageIndex)
{
    submit_render_pass(VulkanInstance, VulkanState, ctk::at(&VulkanState->RenderPasses, "test"), SwapchainImageIndex);
}
