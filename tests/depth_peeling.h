#pragma once

#include "ctk/math.h"

struct depth_peeling_test
{
    ctk::sarray<vtk::image, 4> DepthImages;
    ctk::sarray<vtk::image, 4> ColorImages;
    VkDescriptorPool PresentDescriptorPool;
    VkDescriptorSetLayout PresentDescriptorSetLayout;
    ctk::sarray<VkDescriptorSet, 4> PresentDescriptorSets;
};

static depth_peeling_test DepthPeelingTest = {};

static void
depth_peeling_test_create_state(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    ////////////////////////////////////////////////////////////
    /// Images
    ////////////////////////////////////////////////////////////
    vtk::image_info DepthImageInfo = {};
    DepthImageInfo.Width = Swapchain->Extent.width;
    DepthImageInfo.Height = Swapchain->Extent.height;
    DepthImageInfo.Format = vtk::find_depth_image_format(Device->Physical);
    DepthImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    DepthImageInfo.UsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    DepthImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    DepthImageInfo.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    vtk::image_info ColorImageInfo = {};
    ColorImageInfo.Width = Swapchain->Extent.width;
    ColorImageInfo.Height = Swapchain->Extent.height;
    ColorImageInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
    ColorImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    ColorImageInfo.UsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    ColorImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ColorImageInfo.AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    CTK_REPEAT(Swapchain->Images.Count)
    {
        ctk::push(&DepthPeelingTest.DepthImages, vtk::create_image(Device, &DepthImageInfo));
        ctk::push(&DepthPeelingTest.ColorImages, vtk::create_image(Device, &ColorImageInfo));
    }

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass_info TestRenderPassInfo = {};

    u32 DepthAttachmentIndex = TestRenderPassInfo.Attachments.Count;
    vtk::attachment *DepthAttachment = ctk::push(&TestRenderPassInfo.Attachments);
    DepthAttachment->Description.format = DepthImageInfo.Format;
    DepthAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    DepthAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    DepthAttachment->ClearValue = { 1.0f, 0 };

    u32 ColorAttachmentIndex = TestRenderPassInfo.Attachments.Count;
    vtk::attachment *ColorAttachment = ctk::push(&TestRenderPassInfo.Attachments);
    ColorAttachment->Description.format = ColorImageInfo.Format;
    ColorAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ColorAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ColorAttachment->ClearValue = { 0.04f, 0.04f, 0.04f, 1.0f };

    u32 PresentAttachmentIndex = TestRenderPassInfo.Attachments.Count;
    vtk::attachment *PresentAttachment = ctk::push(&TestRenderPassInfo.Attachments);
    PresentAttachment->Description.format = Swapchain->ImageFormat;
    PresentAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    PresentAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    PresentAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    PresentAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    PresentAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    PresentAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    PresentAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    PresentAttachment->ClearValue = { 0.04f, 0.04f, 0.04f, 1.0f };

    // Subpasses
    vtk::subpass *InitialSubpass = ctk::push(&TestRenderPassInfo.Subpasses);
    ctk::set(&InitialSubpass->DepthAttachmentReference, { DepthAttachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
    ctk::push(&InitialSubpass->ColorAttachmentReferences, { ColorAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    vtk::subpass *PresentSubpass = ctk::push(&TestRenderPassInfo.Subpasses);
    ctk::push(&PresentSubpass->InputAttachmentReferences, { DepthAttachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&PresentSubpass->InputAttachmentReferences, { ColorAttachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&PresentSubpass->ColorAttachmentReferences, { PresentAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    // Subpass Dependencies
    TestRenderPassInfo.SubpassDependencies.Count = 3;

    TestRenderPassInfo.SubpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    TestRenderPassInfo.SubpassDependencies[0].dstSubpass = 0;
    TestRenderPassInfo.SubpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    TestRenderPassInfo.SubpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    TestRenderPassInfo.SubpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    TestRenderPassInfo.SubpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    TestRenderPassInfo.SubpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // This dependency transitions the input attachment from color attachment to shader read
    TestRenderPassInfo.SubpassDependencies[1].srcSubpass = 0;
    TestRenderPassInfo.SubpassDependencies[1].dstSubpass = 1;
    TestRenderPassInfo.SubpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    TestRenderPassInfo.SubpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    TestRenderPassInfo.SubpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    TestRenderPassInfo.SubpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    TestRenderPassInfo.SubpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    TestRenderPassInfo.SubpassDependencies[2].srcSubpass = 0;
    TestRenderPassInfo.SubpassDependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    TestRenderPassInfo.SubpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    TestRenderPassInfo.SubpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    TestRenderPassInfo.SubpassDependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    TestRenderPassInfo.SubpassDependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    TestRenderPassInfo.SubpassDependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Framebuffer Infos
    CTK_REPEAT(Swapchain->Images.Count)
    {
        vtk::framebuffer_info *FramebufferInfo = ctk::push(&TestRenderPassInfo.FramebufferInfos);
        ctk::push(&FramebufferInfo->Attachments, DepthPeelingTest.DepthImages[RepeatIndex].View);
        ctk::push(&FramebufferInfo->Attachments, DepthPeelingTest.ColorImages[RepeatIndex].View);
        ctk::push(&FramebufferInfo->Attachments, Swapchain->Images[RepeatIndex].View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    vtk::render_pass *TestRP = ctk::push(&VulkanState->RenderPasses, "test");
    *TestRP = vtk::create_render_pass(Device->Logical, VulkanInstance->GraphicsCommandPool, &TestRenderPassInfo);

    ////////////////////////////////////////////////////////////
    /// Descriptor Sets
    ////////////////////////////////////////////////////////////

    // Pool
    ctk::sarray<VkDescriptorPoolSize, 4> DescriptorPoolSizes = {};
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, Swapchain->Images.Count * 2 });
    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = {};
    DescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolCreateInfo.flags = 0;
    DescriptorPoolCreateInfo.maxSets = Swapchain->Images.Count;
    DescriptorPoolCreateInfo.poolSizeCount = DescriptorPoolSizes.Count;
    DescriptorPoolCreateInfo.pPoolSizes = DescriptorPoolSizes.Data;
    vtk::validate_vk_result(vkCreateDescriptorPool(Device->Logical, &DescriptorPoolCreateInfo, NULL,
                                                   &DepthPeelingTest.PresentDescriptorPool),
                            "vkCreateDescriptorPool", "failed to create descriptor pool");

    // Layout
    ctk::sarray<VkDescriptorSetLayoutBinding, 4> LayoutBindings = {};
    ctk::push(&LayoutBindings, { 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT });
    ctk::push(&LayoutBindings, { 1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT });

    VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = {};
    DescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    DescriptorSetLayoutCreateInfo.bindingCount = LayoutBindings.Count;
    DescriptorSetLayoutCreateInfo.pBindings = LayoutBindings.Data;
    vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &DescriptorSetLayoutCreateInfo, NULL,
                                                        &DepthPeelingTest.PresentDescriptorSetLayout),
                            "vkCreateDescriptorSetLayout", "error creating descriptor set layout");

    // Allocation
    DepthPeelingTest.PresentDescriptorSets.Count = Swapchain->Images.Count;
    ctk::sarray<VkDescriptorSetLayout, 4> DuplicateLayouts = {};
    CTK_REPEAT(Swapchain->Images.Count)
    {
        ctk::push(&DuplicateLayouts, DepthPeelingTest.PresentDescriptorSetLayout);
    }

    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = {};
    DescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescriptorSetAllocateInfo.descriptorPool = DepthPeelingTest.PresentDescriptorPool;
    DescriptorSetAllocateInfo.descriptorSetCount = DepthPeelingTest.PresentDescriptorSets.Count;
    DescriptorSetAllocateInfo.pSetLayouts = DuplicateLayouts.Data;

    vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &DescriptorSetAllocateInfo, DepthPeelingTest.PresentDescriptorSets.Data),
                            "vkAllocateDescriptorSets", "failed to allocate descriptor sets");

    // Updates
    ctk::sarray<VkDescriptorImageInfo, 16> DescriptorImageInfos = {};
    ctk::sarray<VkWriteDescriptorSet, 16> WriteDescriptorSets = {};
    CTK_REPEAT(Swapchain->Images.Count)
    {
        vtk::image *DepthImage = DepthPeelingTest.DepthImages + RepeatIndex;
        vtk::image *ColorImage = DepthPeelingTest.ColorImages + RepeatIndex;
        VkDescriptorSet PresentDescriptorSet = DepthPeelingTest.PresentDescriptorSets[RepeatIndex];

        VkWriteDescriptorSet *DepthWriteDescriptorSet = ctk::push(&WriteDescriptorSets);
        DepthWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DepthWriteDescriptorSet->dstSet = PresentDescriptorSet;
        DepthWriteDescriptorSet->dstBinding = 0;
        DepthWriteDescriptorSet->dstArrayElement = 0;
        DepthWriteDescriptorSet->descriptorCount = 1;
        DepthWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        DepthWriteDescriptorSet->pImageInfo = ctk::push(&DescriptorImageInfos,
                                                        { VK_NULL_HANDLE, DepthImage->View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

        VkWriteDescriptorSet *ColorWriteDescriptorSet = ctk::push(&WriteDescriptorSets);
        ColorWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ColorWriteDescriptorSet->dstSet = PresentDescriptorSet;
        ColorWriteDescriptorSet->dstBinding = 1;
        ColorWriteDescriptorSet->dstArrayElement = 0;
        ColorWriteDescriptorSet->descriptorCount = 1;
        ColorWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        ColorWriteDescriptorSet->pImageInfo = ctk::push(&DescriptorImageInfos,
                                                        { VK_NULL_HANDLE, ColorImage->View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    }
    vkUpdateDescriptorSets(Device->Logical, WriteDescriptorSets.Count, WriteDescriptorSets.Data, 0, NULL);

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////

    // Blended GP
    vtk::graphics_pipeline_info BlendedGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&BlendedGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_blended_vert"));
    ctk::push(&BlendedGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_blended_frag"));
    ctk::push(&BlendedGPInfo.DescriptorSetLayouts, ctk::at(&VulkanState->DescriptorSets, "entity")->Layout);
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
    BlendedGPInfo.Subpass = 0;
    // BlendedGPInfo.RasterizationState.cullMode = VK_CULL_MODE_NONE;
    vtk::graphics_pipeline *BlendedGP = ctk::push(&VulkanState->GraphicsPipelines, "blended",
                                                  vtk::create_graphics_pipeline(Device->Logical, TestRP, &BlendedGPInfo));

    // Present GP
    vtk::graphics_pipeline_info PresentGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&PresentGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_present_vert"));
    ctk::push(&PresentGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_present_frag"));
    ctk::push(&PresentGPInfo.DescriptorSetLayouts, DepthPeelingTest.PresentDescriptorSetLayout);
    ctk::push(&PresentGPInfo.VertexInputs, { 0, 0, *ctk::at(&VulkanState->VertexAttributeIndexes, "position") });
    PresentGPInfo.VertexLayout = &VulkanState->VertexLayout;
    ctk::push(&PresentGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&PresentGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&PresentGPInfo.ColorBlendAttachmentStates,
              {
                  VK_FALSE,
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
    PresentGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    PresentGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    PresentGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    // PresentGPInfo.RasterizationState.cullMode = VK_CULL_MODE_NONE;
    PresentGPInfo.Subpass = 1;
    vtk::graphics_pipeline *PresentGP = ctk::push(&VulkanState->GraphicsPipelines, "present",
                                                  vtk::create_graphics_pipeline(Device->Logical, TestRP, &PresentGPInfo));
}

static scene *
depth_peeling_test_create_scene(assets *Assets, vulkan_state *VulkanState)
{
    scene *Scene = create_scene(Assets, VulkanState, "assets/scenes/empty.ctkd");
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
depth_peeling_test_init(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState, scene *Scene)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    vtk::render_pass *RenderPass = ctk::at(&VulkanState->RenderPasses, "test");

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
        VkCommandBuffer CommandBuffer = *ctk::at(&RenderPass->CommandBuffers, SwapchainImageIndex);
        vtk::validate_vk_result(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                                "vkBeginCommandBuffer", "failed to begin recording command buffer");
        VkRenderPassBeginInfo RenderPassBeginInfo = {};
        RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassBeginInfo.renderPass = RenderPass->Handle;
        RenderPassBeginInfo.framebuffer = *ctk::at(&RenderPass->Framebuffers, SwapchainImageIndex);
        RenderPassBeginInfo.renderArea = RenderArea;
        RenderPassBeginInfo.clearValueCount = RenderPass->ClearValues.Count;
        RenderPassBeginInfo.pClearValues = RenderPass->ClearValues.Data;

        // Begin
        vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            // First subpass, draws all entities to framebuffer color & depth attachments.
            for(u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex)
            {
                entity *Entity = Scene->Entities.Values + EntityIndex;
                mesh *Mesh = Entity->Mesh;

                // Graphics Pipeline
                vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Entity->GraphicsPipeline->Handle);

                // Descriptor Sets
                vtk::bind_descriptor_sets(CommandBuffer, Entity->GraphicsPipeline->Layout,
                                          Entity->DescriptorSets.Data, Entity->DescriptorSets.Count,
                                          SwapchainImageIndex, EntityIndex);

                // Vertex/Index Buffers
                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
                vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);

                // Draw
                vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
            }

            // Second subpass, reads framebuffer color & depth attachments written to in first pass and renders to present attachment.
            vtk::graphics_pipeline *PresentGP = ctk::at(&VulkanState->GraphicsPipelines, "present");
            mesh *FullscreenPlane = ctk::at(&Assets->Meshes, "fullscreen_plane");
            vkCmdNextSubpass(CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PresentGP->Handle);
            vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PresentGP->Layout,
                                    0, 1, DepthPeelingTest.PresentDescriptorSets + SwapchainImageIndex,
                                    0, NULL);
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &FullscreenPlane->VertexRegion.Buffer->Handle, &FullscreenPlane->VertexRegion.Offset);
            vkCmdBindIndexBuffer(CommandBuffer, FullscreenPlane->IndexRegion.Buffer->Handle, FullscreenPlane->IndexRegion.Offset,
                                 VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(CommandBuffer, FullscreenPlane->Indexes.Count, 1, 0, 0, 0);

        // End
        vkCmdEndRenderPass(CommandBuffer);
        vtk::validate_vk_result(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
    }
}

static void
depth_peeling_test_update(vulkan_instance *VulkanInstance, vulkan_state *VulkanState, u32 SwapchainImageIndex)
{
    submit_render_pass(VulkanInstance, VulkanState, ctk::at(&VulkanState->RenderPasses, "test"), SwapchainImageIndex);
}
