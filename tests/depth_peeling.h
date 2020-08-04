#pragma once

#include "gfx/tests/shared.h"

struct state {
    scene *Scene;
    vtk::image DepthAttachmentImage;
    // vtk::image ColorAttachmentImage;
    vtk::render_pass RenderPass;
    VkDescriptorPool DescriptorPool;
    struct {
        VkDescriptorSetLayout EntityUniformBuffer;
    } DescriptorSetLayouts;
    struct {
        vtk::descriptor_set EntityUniformBuffer;
    } DescriptorSets;
    vtk::vertex_layout VertexLayout;
    struct {
        u32 Position;
        u32 Normal;
        u32 UV;
    } VertexAttributeIndexes;
    struct {
        vtk::graphics_pipeline Blend;
    } GraphicsPipelines;
};

static void create_vulkan_state(state *State, vulkan_instance *VulkanInstance, assets *Assets) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    ////////////////////////////////////////////////////////////
    /// Attachment Images
    ////////////////////////////////////////////////////////////
    vtk::image_info DepthImageInfo = {};
    DepthImageInfo.Width = Swapchain->Extent.width;
    DepthImageInfo.Height = Swapchain->Extent.height;
    DepthImageInfo.Format = vtk::find_depth_image_format(Device->Physical);
    DepthImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    DepthImageInfo.UsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT/* | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT*/;
    DepthImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    DepthImageInfo.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    State->DepthAttachmentImage = vtk::create_image(Device, &DepthImageInfo);

    // vtk::image_info ColorImageInfo = {};
    // ColorImageInfo.Width = Swapchain->Extent.width;
    // ColorImageInfo.Height = Swapchain->Extent.height;
    // ColorImageInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
    // ColorImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    // ColorImageInfo.UsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    // ColorImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    // ColorImageInfo.AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // State->ColorImage = vtk::create_image(Device, &ColorImageInfo);

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass_info RenderPassInfo = {};

    // Attachments
    u32 DepthAttachmentIndex = RenderPassInfo.Attachments.Count;
    vtk::attachment *DepthAttachment = ctk::push(&RenderPassInfo.Attachments);
    DepthAttachment->Description.format = DepthImageInfo.Format;
    DepthAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    DepthAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    DepthAttachment->ClearValue = { 1.0f, 0 };

    u32 ColorAttachmentIndex = RenderPassInfo.Attachments.Count;
    vtk::attachment *ColorAttachment = ctk::push(&RenderPassInfo.Attachments);
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
    vtk::subpass *Subpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::set(&Subpass->DepthAttachmentReference, { DepthAttachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
    ctk::push(&Subpass->ColorAttachmentReferences, { ColorAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    // Subpass Dependencies
    RenderPassInfo.SubpassDependencies.Count = 1;
    RenderPassInfo.SubpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    RenderPassInfo.SubpassDependencies[0].dstSubpass = 0;
    RenderPassInfo.SubpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    RenderPassInfo.SubpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    RenderPassInfo.SubpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    RenderPassInfo.SubpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    RenderPassInfo.SubpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Framebuffer Infos
    CTK_REPEAT(Swapchain->Images.Count) {
        vtk::framebuffer_info *FramebufferInfo = ctk::push(&RenderPassInfo.FramebufferInfos);
        ctk::push(&FramebufferInfo->Attachments, State->DepthAttachmentImage.View);
        ctk::push(&FramebufferInfo->Attachments, Swapchain->Images[RepeatIndex].View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    State->RenderPass = vtk::create_render_pass(Device->Logical, VulkanInstance->GraphicsCommandPool, &RenderPassInfo);

    ////////////////////////////////////////////////////////////
    /// Descriptor Sets
    ////////////////////////////////////////////////////////////

    // Pool
    ctk::sarray<VkDescriptorPoolSize, 4> DescriptorPoolSizes = {};
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, Swapchain->Images.Count });
    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = {};
    DescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolCreateInfo.flags = 0;
    DescriptorPoolCreateInfo.maxSets = Swapchain->Images.Count;
    DescriptorPoolCreateInfo.poolSizeCount = DescriptorPoolSizes.Count;
    DescriptorPoolCreateInfo.pPoolSizes = DescriptorPoolSizes.Data;
    vtk::validate_vk_result(vkCreateDescriptorPool(Device->Logical, &DescriptorPoolCreateInfo, NULL, &State->DescriptorPool),
                            "vkCreateDescriptorPool", "failed to create descriptor pool");

    // Layouts
    ctk::sarray<VkDescriptorSetLayoutBinding, 4> LayoutBindings = {};
    ctk::push(&LayoutBindings, { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT });

    VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = {};
    DescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    DescriptorSetLayoutCreateInfo.bindingCount = LayoutBindings.Count;
    DescriptorSetLayoutCreateInfo.pBindings = LayoutBindings.Data;
    vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &DescriptorSetLayoutCreateInfo, NULL,
                                                        &State->DescriptorSetLayouts.EntityUniformBuffer),
                            "vkCreateDescriptorSetLayout", "error creating descriptor set layout");

    // Initialization
    vtk::descriptor_set *EntityUniformBufferDS = &State->DescriptorSets.EntityUniformBuffer;
    EntityUniformBufferDS->Instances.Count = Swapchain->Images.Count;
    ctk::push(&EntityUniformBufferDS->DynamicOffsets, State->Scene->EntityUniformBuffer.ElementSize);

    // Allocation
    ctk::sarray<VkDescriptorSetLayout, 4> DuplicateLayouts = {};
    CTK_REPEAT(EntityUniformBufferDS->Instances.Count) ctk::push(&DuplicateLayouts, State->DescriptorSetLayouts.EntityUniformBuffer);

    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = {};
    DescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescriptorSetAllocateInfo.descriptorPool = State->DescriptorPool;
    DescriptorSetAllocateInfo.descriptorSetCount = EntityUniformBufferDS->Instances.Count;
    DescriptorSetAllocateInfo.pSetLayouts = DuplicateLayouts.Data;

    vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &DescriptorSetAllocateInfo, EntityUniformBufferDS->Instances.Data),
                            "vkAllocateDescriptorSets", "failed to allocate descriptor sets");

    // Updates
    ctk::sarray<VkDescriptorBufferInfo, 16> DescriptorBufferInfos = {};
    ctk::sarray<VkDescriptorImageInfo, 16> DescriptorImageInfos = {};
    ctk::sarray<VkWriteDescriptorSet, 16> WriteDescriptorSets = {};
    CTK_REPEAT(EntityUniformBufferDS->Instances.Count) {
        // vtk::image *DepthImage = State->DepthImages + RepeatIndex;
        // vtk::image *ColorImage = State->ColorImages + RepeatIndex;
        VkDescriptorSet DescriptorSet = EntityUniformBufferDS->Instances[RepeatIndex];

        // VkWriteDescriptorSet *DepthWriteDescriptorSet = ctk::push(&WriteDescriptorSets);
        // DepthWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        // DepthWriteDescriptorSet->dstSet = DescriptorSet;
        // DepthWriteDescriptorSet->dstBinding = 0;
        // DepthWriteDescriptorSet->dstArrayElement = 0;
        // DepthWriteDescriptorSet->descriptorCount = 1;
        // DepthWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        // DepthWriteDescriptorSet->pImageInfo = ctk::push(&DescriptorImageInfos,
        //                                                 { VK_NULL_HANDLE, DepthImage->View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

        // VkWriteDescriptorSet *ColorWriteDescriptorSet = ctk::push(&WriteDescriptorSets);
        // ColorWriteDescriptorSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        // ColorWriteDescriptorSet->dstSet = DescriptorSet;
        // ColorWriteDescriptorSet->dstBinding = 1;
        // ColorWriteDescriptorSet->dstArrayElement = 0;
        // ColorWriteDescriptorSet->descriptorCount = 1;
        // ColorWriteDescriptorSet->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        // ColorWriteDescriptorSet->pImageInfo = ctk::push(&DescriptorImageInfos,
        //                                                 { VK_NULL_HANDLE, ColorImage->View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

        // Entity Uniform Buffer
        vtk::region *EntityUniformBufferRegion = State->Scene->EntityUniformBuffer.Regions + RepeatIndex;

        VkDescriptorBufferInfo *EntityUniformBufferInfo = ctk::push(&DescriptorBufferInfos);
        EntityUniformBufferInfo->buffer = EntityUniformBufferRegion->Buffer->Handle;
        EntityUniformBufferInfo->offset = EntityUniformBufferRegion->Offset;
        EntityUniformBufferInfo->range = EntityUniformBufferRegion->Size;

        VkWriteDescriptorSet *EntityUniformBufferWrite = ctk::push(&WriteDescriptorSets);
        EntityUniformBufferWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        EntityUniformBufferWrite->dstSet = DescriptorSet;
        EntityUniformBufferWrite->dstBinding = 0;
        EntityUniformBufferWrite->dstArrayElement = 0;
        EntityUniformBufferWrite->descriptorCount = 1;
        EntityUniformBufferWrite->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        EntityUniformBufferWrite->pBufferInfo = EntityUniformBufferInfo;
    }
    vkUpdateDescriptorSets(Device->Logical, WriteDescriptorSets.Count, WriteDescriptorSets.Data, 0, NULL);

    ////////////////////////////////////////////////////////////
    /// Vertex Layout
    ////////////////////////////////////////////////////////////
    State->VertexAttributeIndexes.Position = vtk::push_vertex_attribute(&State->VertexLayout, 3);
    State->VertexAttributeIndexes.Normal = vtk::push_vertex_attribute(&State->VertexLayout, 3);
    State->VertexAttributeIndexes.UV = vtk::push_vertex_attribute(&State->VertexLayout, 2);

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////

    // Blend GP
    vtk::graphics_pipeline_info BlendGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&BlendGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_blend_vert"));
    ctk::push(&BlendGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_blend_frag"));
    ctk::push(&BlendGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.EntityUniformBuffer);
    ctk::push(&BlendGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    BlendGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&BlendGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&BlendGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&BlendGPInfo.ColorBlendAttachmentStates, {
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
    BlendGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    BlendGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    BlendGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    BlendGPInfo.Subpass = 0;
    // BlendGPInfo.RasterizationState.cullMode = VK_CULL_MODE_NONE;
    State->GraphicsPipelines.Blend = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, &BlendGPInfo);
}

static void create_entities(state *State, vulkan_instance *VulkanInstance, assets *Assets) {
    CTK_REPEAT(4) {
        char Name[16] = {};
        sprintf(Name, "cube_%u", RepeatIndex);
        entity *Cube = push_entity(State->Scene, Name);
        Cube->Transform.Position = { 0, 0, 1.5f * RepeatIndex };
        Cube->Transform.Scale = { 1, 1, 1 };
        Cube->Mesh = ctk::at(&Assets->Meshes, "cube");
    }
}

static state *create_state(vulkan_instance *VulkanInstance, assets *Assets) {
    auto State = ctk::allocate<state>();
    *State = {};
    State->Scene = create_scene(Assets, VulkanInstance, "assets/scenes/empty.ctkd");
    create_entities(State, VulkanInstance, Assets);
    create_vulkan_state(State, VulkanInstance, Assets);
    return State;
}

static void record_render_command_buffers(state *State, vulkan_instance *VulkanInstance, assets *Assets) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    vtk::render_pass *RenderPass = &State->RenderPass;

    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = Swapchain->Extent;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = 0;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

    for(u32 SwapchainImageIndex = 0; SwapchainImageIndex < Swapchain->Images.Count; ++SwapchainImageIndex) {
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

        vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); {
            vtk::graphics_pipeline *BlendGP = &State->GraphicsPipelines.Blend;
            vtk::descriptor_set *DescriptorSets[] { &State->DescriptorSets.EntityUniformBuffer };
            for(u32 EntityIndex = 0; EntityIndex < State->Scene->Entities.Count; ++EntityIndex) {
                entity *Entity = State->Scene->Entities.Values + EntityIndex;
                mesh *Mesh = Entity->Mesh;

                // Graphics Pipeline
                vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, BlendGP->Handle);

                // Descriptor Sets
                vtk::bind_descriptor_sets(CommandBuffer, BlendGP->Layout, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets),
                                          SwapchainImageIndex, EntityIndex);

                // Vertex/Index Buffers
                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
                vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);

                // Draw
                vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
            }
        } vkCmdEndRenderPass(CommandBuffer);
        vtk::validate_vk_result(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
    }
}

static void depth_peeling_test_main() {
    input_state InputState = {};
    window *Window = create_window(&InputState);
    vulkan_instance *VulkanInstance = create_vulkan_instance(Window);
    assets *Assets = create_assets(VulkanInstance);
    state *State = create_state(VulkanInstance, Assets);
    record_render_command_buffers(State, VulkanInstance, Assets);
    while(!glfwWindowShouldClose(Window->Handle)) {
        // Check if window should close.
        glfwPollEvents();
        if(InputState.KeyDown[GLFW_KEY_ESCAPE]) break;

        // Frame processing.
        update_input_state(&InputState, Window->Handle);
        camera_controls(&State->Scene->Camera.Transform, &InputState);
        u32 SwapchainImageIndex = aquire_next_swapchain_image_index(VulkanInstance);
        update_entity_uniform_buffer(VulkanInstance, State->Scene, SwapchainImageIndex);
        synchronize_current_frame(VulkanInstance, SwapchainImageIndex);
        submit_render_pass(VulkanInstance, &State->RenderPass, SwapchainImageIndex);
        cycle_frame(VulkanInstance);
        Sleep(1);
    }
}
