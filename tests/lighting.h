#pragma once

#include "gfx/tests/shared.h"

struct state {
    scene Scene;
    vtk::render_pass RenderPass;
    VkDescriptorPool DescriptorPool;
    vtk::vertex_layout VertexLayout;
    enum {
        LIGHT_MODE_COMPOSITE,
        LIGHT_MODE_ALBEDO,
        LIGHT_MODE_POSITION,
        LIGHT_MODE_NORMAL,
    } LightMode;
    struct {
        u32 Position;
        u32 Normal;
        u32 UV;
    } VertexAttributeIndexes;
    struct {
        vtk::uniform_buffer EntityMatrixes;
        vtk::uniform_buffer LightMatrixes;
        vtk::uniform_buffer Lights;
    } UniformBuffers;
    struct {
        vtk::image Albedo;
        vtk::image Position;
        vtk::image Normal;
        vtk::image Depth;
    } AttachmentImages;
    struct {
        VkDescriptorSetLayout EntityMatrixes;
        VkDescriptorSetLayout Lights;
        VkDescriptorSetLayout InputAttachments;
        VkDescriptorSetLayout Textures;
    } DescriptorSetLayouts;
    struct {
        vtk::descriptor_set EntityMatrixes;
        vtk::descriptor_set LightMatrixes;
        vtk::descriptor_set Lights;
        vtk::descriptor_set InputAttachments;
        ctk::smap<vtk::descriptor_set, 16> Textures;
    } DescriptorSets;
    struct {
        vtk::graphics_pipeline Deferred;
        vtk::graphics_pipeline Lighting;
        vtk::graphics_pipeline UnlitColor;
    } GraphicsPipelines;
};

static void create_vulkan_state(state *State, vulkan_instance *VulkanInstance, assets *Assets) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    ////////////////////////////////////////////////////////////
    /// Vertex Layout
    ////////////////////////////////////////////////////////////
    State->VertexAttributeIndexes.Position = vtk::push_vertex_attribute(&State->VertexLayout, 3);
    State->VertexAttributeIndexes.Normal = vtk::push_vertex_attribute(&State->VertexLayout, 3);
    State->VertexAttributeIndexes.UV = vtk::push_vertex_attribute(&State->VertexLayout, 2);

    ////////////////////////////////////////////////////////////
    /// Buffers
    ////////////////////////////////////////////////////////////
    State->UniformBuffers.EntityMatrixes = vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, scene::MAX_ENTITIES, sizeof(matrix_ubo),
                                                                      Swapchain->Images.Count);
    State->UniformBuffers.LightMatrixes = vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, scene::MAX_LIGHTS, sizeof(matrix_ubo),
                                                                     Swapchain->Images.Count);
    ctk::todo("HACK: hardcoded lights uniform buffer to satisfy alignment requirements");
    State->UniformBuffers.Lights = vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, 1, 528, Swapchain->Images.Count);

    ////////////////////////////////////////////////////////////
    /// Attachment Images
    ////////////////////////////////////////////////////////////
    vtk::image_info DepthImageInfo = {};
    DepthImageInfo.Width = Swapchain->Extent.width;
    DepthImageInfo.Height = Swapchain->Extent.height;
    DepthImageInfo.Format = vtk::find_depth_image_format(Device->Physical);
    DepthImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    DepthImageInfo.UsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    DepthImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    DepthImageInfo.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    State->AttachmentImages.Depth = vtk::create_image(Device, &DepthImageInfo);

    vtk::image_info ColorImageInfo = {};
    ColorImageInfo.Width = Swapchain->Extent.width;
    ColorImageInfo.Height = Swapchain->Extent.height;
    ColorImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    ColorImageInfo.UsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    ColorImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ColorImageInfo.AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ColorImageInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
    State->AttachmentImages.Albedo = vtk::create_image(Device, &ColorImageInfo);
    ColorImageInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
    State->AttachmentImages.Position = vtk::create_image(Device, &ColorImageInfo);
    State->AttachmentImages.Normal = vtk::create_image(Device, &ColorImageInfo);

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass_info RenderPassInfo = {};

    // Attachments
    vtk::attachment *AlbedoAttachment = ctk::push(&RenderPassInfo.Attachments);
    AlbedoAttachment->Description.format = VK_FORMAT_R8G8B8A8_UNORM;
    AlbedoAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    AlbedoAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    AlbedoAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    AlbedoAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    AlbedoAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    AlbedoAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    AlbedoAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    AlbedoAttachment->ClearValue = { 0, 0, 0, 0 };

    vtk::attachment *PositionAttachment = ctk::push(&RenderPassInfo.Attachments);
    PositionAttachment->Description.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    PositionAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    PositionAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    PositionAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    PositionAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    PositionAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    PositionAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    PositionAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    PositionAttachment->ClearValue = { 0, 0, 0, 0 };

    vtk::attachment *NormalAttachment = ctk::push(&RenderPassInfo.Attachments);
    NormalAttachment->Description.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    NormalAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    NormalAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    NormalAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    NormalAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    NormalAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    NormalAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    NormalAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    NormalAttachment->ClearValue = { 0, 0, 0, 0 };

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

    vtk::attachment *SwapchainAttachment = ctk::push(&RenderPassInfo.Attachments);
    SwapchainAttachment->Description.format = Swapchain->ImageFormat;
    SwapchainAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    SwapchainAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    SwapchainAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    SwapchainAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    SwapchainAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    SwapchainAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    SwapchainAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    SwapchainAttachment->ClearValue = { 0, 0, 0, 0 };

    // Subpasses
    vtk::subpass *DirectSubpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::push(&DirectSubpass->ColorAttachmentReferences, { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::set(&DirectSubpass->DepthAttachmentReference, { 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });

    vtk::subpass *DeferredSubpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::set(&DeferredSubpass->DepthAttachmentReference, { 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });

    vtk::subpass *LightingSubpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->ColorAttachmentReferences, { 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    // Subpass Dependencies
    RenderPassInfo.SubpassDependencies.Count = 3;
    RenderPassInfo.SubpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    RenderPassInfo.SubpassDependencies[0].dstSubpass = 0;
    RenderPassInfo.SubpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    RenderPassInfo.SubpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    RenderPassInfo.SubpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    RenderPassInfo.SubpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    RenderPassInfo.SubpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    RenderPassInfo.SubpassDependencies[1].srcSubpass = 0;
    RenderPassInfo.SubpassDependencies[1].dstSubpass = 1;
    RenderPassInfo.SubpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    RenderPassInfo.SubpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    RenderPassInfo.SubpassDependencies[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    RenderPassInfo.SubpassDependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    RenderPassInfo.SubpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    RenderPassInfo.SubpassDependencies[2].srcSubpass = 1;
    RenderPassInfo.SubpassDependencies[2].dstSubpass = 2;
    RenderPassInfo.SubpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    RenderPassInfo.SubpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    RenderPassInfo.SubpassDependencies[2].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    RenderPassInfo.SubpassDependencies[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    RenderPassInfo.SubpassDependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Framebuffer Infos
    CTK_ITERATE(Swapchain->Images.Count) {
        vtk::framebuffer_info *FramebufferInfo = ctk::push(&RenderPassInfo.FramebufferInfos);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Albedo.View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Position.View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Normal.View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Depth.View);
        ctk::push(&FramebufferInfo->Attachments, Swapchain->Images[IterationIndex].View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    State->RenderPass = vtk::create_render_pass(Device->Logical, VulkanInstance->GraphicsCommandPool, &RenderPassInfo);

    ////////////////////////////////////////////////////////////
    /// Descriptor Sets
    ////////////////////////////////////////////////////////////
    vtk::descriptor_set *EntityMatrixesDS = &State->DescriptorSets.EntityMatrixes;
    vtk::descriptor_set *LightMatrixesDS = &State->DescriptorSets.LightMatrixes;
    vtk::descriptor_set *LightsDS = &State->DescriptorSets.Lights;
    vtk::descriptor_set *InputAttachmentsDS = &State->DescriptorSets.InputAttachments;

    // Pool
    ctk::sarray<VkDescriptorPoolSize, 8> DescriptorPoolSizes = {};

    // One for each uniform buffer x swapchain image.
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, Swapchain->Images.Count * 2 });
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain->Images.Count });
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 3 });
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 });
    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = {};
    DescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolCreateInfo.flags = 0;
    DescriptorPoolCreateInfo.maxSets = 32;
    DescriptorPoolCreateInfo.poolSizeCount = DescriptorPoolSizes.Count;
    DescriptorPoolCreateInfo.pPoolSizes = DescriptorPoolSizes.Data;
    vtk::validate_vk_result(vkCreateDescriptorPool(Device->Logical, &DescriptorPoolCreateInfo, NULL, &State->DescriptorPool),
                            "vkCreateDescriptorPool", "failed to create descriptor pool");

    // Layouts

    // EntityMatrixes
    ctk::sarray<VkDescriptorSetLayoutBinding, 4> EntityMatrixesLayoutBindings = {};
    ctk::push(&EntityMatrixesLayoutBindings, { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT });

    VkDescriptorSetLayoutCreateInfo EntityMatrixesLayoutCreateInfo = {};
    EntityMatrixesLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    EntityMatrixesLayoutCreateInfo.bindingCount = EntityMatrixesLayoutBindings.Count;
    EntityMatrixesLayoutCreateInfo.pBindings = EntityMatrixesLayoutBindings.Data;
    vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &EntityMatrixesLayoutCreateInfo, NULL,
                                                        &State->DescriptorSetLayouts.EntityMatrixes),
                            "vkCreateDescriptorSetLayout", "error creating descriptor set layout");


    // Lights
    ctk::sarray<VkDescriptorSetLayoutBinding, 4> LightsLayoutBindings = {};
    ctk::push(&LightsLayoutBindings, { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT });

    VkDescriptorSetLayoutCreateInfo LightsLayoutCreateInfo = {};
    LightsLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    LightsLayoutCreateInfo.bindingCount = LightsLayoutBindings.Count;
    LightsLayoutCreateInfo.pBindings = LightsLayoutBindings.Data;
    vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &LightsLayoutCreateInfo, NULL,
                                                        &State->DescriptorSetLayouts.Lights),
                            "vkCreateDescriptorSetLayout", "error creating descriptor set layout");

    // InputAttachments
    ctk::sarray<VkDescriptorSetLayoutBinding, 4> InputAttachmentsLayoutBindings = {};
    ctk::push(&InputAttachmentsLayoutBindings, { 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT });
    ctk::push(&InputAttachmentsLayoutBindings, { 1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT });
    ctk::push(&InputAttachmentsLayoutBindings, { 2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT });

    VkDescriptorSetLayoutCreateInfo InputAttachmentsLayoutCreateInfo = {};
    InputAttachmentsLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    InputAttachmentsLayoutCreateInfo.bindingCount = InputAttachmentsLayoutBindings.Count;
    InputAttachmentsLayoutCreateInfo.pBindings = InputAttachmentsLayoutBindings.Data;
    vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &InputAttachmentsLayoutCreateInfo, NULL,
                                                        &State->DescriptorSetLayouts.InputAttachments),
                            "vkCreateDescriptorSetLayout", "error creating descriptor set layout");

    // Textures
    ctk::sarray<VkDescriptorSetLayoutBinding, 4> TexturesLayoutBindings = {};
    ctk::push(&TexturesLayoutBindings, { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT });

    VkDescriptorSetLayoutCreateInfo TexturesLayoutCreateInfo = {};
    TexturesLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    TexturesLayoutCreateInfo.bindingCount = TexturesLayoutBindings.Count;
    TexturesLayoutCreateInfo.pBindings = TexturesLayoutBindings.Data;
    vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &TexturesLayoutCreateInfo, NULL,
                                                        &State->DescriptorSetLayouts.Textures),
                            "vkCreateDescriptorSetLayout", "error creating descriptor set layout");

    // Allocation
    ctk::sarray<VkDescriptorSetLayout, 4> EntityMatrixesDuplicateLayouts = {};
    CTK_ITERATE(Swapchain->Images.Count) ctk::push(&EntityMatrixesDuplicateLayouts, State->DescriptorSetLayouts.EntityMatrixes);

    // EntityMatrixes
    EntityMatrixesDS->Instances.Count = Swapchain->Images.Count;
    ctk::push(&EntityMatrixesDS->DynamicOffsets, State->UniformBuffers.EntityMatrixes.ElementSize);

    VkDescriptorSetAllocateInfo EntityMatrixesAllocateInfo = {};
    EntityMatrixesAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    EntityMatrixesAllocateInfo.descriptorPool = State->DescriptorPool;
    EntityMatrixesAllocateInfo.descriptorSetCount = EntityMatrixesDuplicateLayouts.Count;
    EntityMatrixesAllocateInfo.pSetLayouts = EntityMatrixesDuplicateLayouts.Data;
    vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &EntityMatrixesAllocateInfo, EntityMatrixesDS->Instances.Data),
                            "vkAllocateDescriptorSets", "failed to allocate descriptor sets");

    // LightMatrixes
    LightMatrixesDS->Instances.Count = Swapchain->Images.Count;
    ctk::push(&LightMatrixesDS->DynamicOffsets, State->UniformBuffers.LightMatrixes.ElementSize);

    VkDescriptorSetAllocateInfo LightMatrixesAllocateInfo = {};
    LightMatrixesAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    LightMatrixesAllocateInfo.descriptorPool = State->DescriptorPool;
    LightMatrixesAllocateInfo.descriptorSetCount = EntityMatrixesDuplicateLayouts.Count;
    LightMatrixesAllocateInfo.pSetLayouts = EntityMatrixesDuplicateLayouts.Data;
    vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &LightMatrixesAllocateInfo, LightMatrixesDS->Instances.Data),
                            "vkAllocateDescriptorSets", "failed to allocate descriptor sets");

    // Lights
    LightsDS->Instances.Count = Swapchain->Images.Count;

    ctk::sarray<VkDescriptorSetLayout, 4> LightsDuplicateLayouts = {};
    CTK_ITERATE(Swapchain->Images.Count) ctk::push(&LightsDuplicateLayouts, State->DescriptorSetLayouts.Lights);

    VkDescriptorSetAllocateInfo LightsAllocateInfo = {};
    LightsAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    LightsAllocateInfo.descriptorPool = State->DescriptorPool;
    LightsAllocateInfo.descriptorSetCount = LightsDuplicateLayouts.Count;
    LightsAllocateInfo.pSetLayouts = LightsDuplicateLayouts.Data;
    vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &LightsAllocateInfo, LightsDS->Instances.Data),
                            "vkAllocateDescriptorSets", "failed to allocate descriptor sets");

    // InputAttachments
    InputAttachmentsDS->Instances.Count = 1;
    VkDescriptorSetAllocateInfo InputAttachmentsAllocateInfo = {};
    InputAttachmentsAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    InputAttachmentsAllocateInfo.descriptorPool = State->DescriptorPool;
    InputAttachmentsAllocateInfo.descriptorSetCount = 1;
    InputAttachmentsAllocateInfo.pSetLayouts = &State->DescriptorSetLayouts.InputAttachments;
    vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &InputAttachmentsAllocateInfo, InputAttachmentsDS->Instances.Data),
                            "vkAllocateDescriptorSets", "failed to allocate descriptor sets");

    // Textures
    CTK_ITERATE(Assets->Textures.Count) {
        vtk::descriptor_set *TexturesDS = ctk::push(&State->DescriptorSets.Textures, Assets->Textures.Keys[IterationIndex]);
        TexturesDS->Instances.Count = 1;

        VkDescriptorSetAllocateInfo TexturesAllocateInfo = {};
        TexturesAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        TexturesAllocateInfo.descriptorPool = State->DescriptorPool;
        TexturesAllocateInfo.descriptorSetCount = TexturesDS->Instances.Count;
        TexturesAllocateInfo.pSetLayouts = &State->DescriptorSetLayouts.Textures;
        vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &TexturesAllocateInfo, TexturesDS->Instances.Data),
                                "vkAllocateDescriptorSets", "failed to allocate descriptor sets");
    }

    // Updates
    ctk::sarray<VkDescriptorBufferInfo, 32> DescriptorBufferInfos = {};
    ctk::sarray<VkDescriptorImageInfo, 32> DescriptorImageInfos = {};
    ctk::sarray<VkWriteDescriptorSet, 32> WriteDescriptorSets = {};

    // EntityMatrixes
    CTK_ITERATE(EntityMatrixesDS->Instances.Count) {
        vtk::region *EntityMatrixesRegion = State->UniformBuffers.EntityMatrixes.Regions + IterationIndex;

        VkDescriptorBufferInfo *EntityMatrixDescriptorBufferInfo = ctk::push(&DescriptorBufferInfos);
        EntityMatrixDescriptorBufferInfo->buffer = EntityMatrixesRegion->Buffer->Handle;
        EntityMatrixDescriptorBufferInfo->offset = EntityMatrixesRegion->Offset;
        EntityMatrixDescriptorBufferInfo->range = EntityMatrixesRegion->Size;

        VkWriteDescriptorSet *EntityMatrixesWrite = ctk::push(&WriteDescriptorSets);
        EntityMatrixesWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        EntityMatrixesWrite->dstSet = EntityMatrixesDS->Instances[IterationIndex];
        EntityMatrixesWrite->dstBinding = 0;
        EntityMatrixesWrite->dstArrayElement = 0;
        EntityMatrixesWrite->descriptorCount = 1;
        EntityMatrixesWrite->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        EntityMatrixesWrite->pBufferInfo = EntityMatrixDescriptorBufferInfo;
    }

    // LightMatrixes
    CTK_ITERATE(LightMatrixesDS->Instances.Count) {
        vtk::region *LightMatrixesRegion = State->UniformBuffers.LightMatrixes.Regions + IterationIndex;

        VkDescriptorBufferInfo *LightMatrixDescriptorBufferInfo = ctk::push(&DescriptorBufferInfos);
        LightMatrixDescriptorBufferInfo->buffer = LightMatrixesRegion->Buffer->Handle;
        LightMatrixDescriptorBufferInfo->offset = LightMatrixesRegion->Offset;
        LightMatrixDescriptorBufferInfo->range = LightMatrixesRegion->Size;

        VkWriteDescriptorSet *LightMatrixesWrite = ctk::push(&WriteDescriptorSets);
        LightMatrixesWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        LightMatrixesWrite->dstSet = LightMatrixesDS->Instances[IterationIndex];
        LightMatrixesWrite->dstBinding = 0;
        LightMatrixesWrite->dstArrayElement = 0;
        LightMatrixesWrite->descriptorCount = 1;
        LightMatrixesWrite->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        LightMatrixesWrite->pBufferInfo = LightMatrixDescriptorBufferInfo;
    }

    // Lights
    CTK_ITERATE(LightsDS->Instances.Count) {
        vtk::region *LightsRegion = State->UniformBuffers.Lights.Regions + IterationIndex;

        VkDescriptorBufferInfo *LightsDescriptorBufferInfo = ctk::push(&DescriptorBufferInfos);
        LightsDescriptorBufferInfo->buffer = LightsRegion->Buffer->Handle;
        LightsDescriptorBufferInfo->offset = LightsRegion->Offset;
        LightsDescriptorBufferInfo->range = LightsRegion->Size;

        VkWriteDescriptorSet *LightsWrite = ctk::push(&WriteDescriptorSets);
        LightsWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        LightsWrite->dstSet = LightsDS->Instances[IterationIndex];
        LightsWrite->dstBinding = 0;
        LightsWrite->dstArrayElement = 0;
        LightsWrite->descriptorCount = 1;
        LightsWrite->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        LightsWrite->pBufferInfo = LightsDescriptorBufferInfo;
    }

    // InputAttachments
    VkDescriptorImageInfo *AlbedoInputDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
    AlbedoInputDescriptorImageInfo->sampler = VK_NULL_HANDLE;
    AlbedoInputDescriptorImageInfo->imageView = State->AttachmentImages.Albedo.View;
    AlbedoInputDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet *AlbedoInputImageWrite = ctk::push(&WriteDescriptorSets);
    AlbedoInputImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    AlbedoInputImageWrite->dstSet = InputAttachmentsDS->Instances[0];
    AlbedoInputImageWrite->dstBinding = 0;
    AlbedoInputImageWrite->dstArrayElement = 0;
    AlbedoInputImageWrite->descriptorCount = 1;
    AlbedoInputImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    AlbedoInputImageWrite->pImageInfo = AlbedoInputDescriptorImageInfo;

    VkDescriptorImageInfo *PositionInputDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
    PositionInputDescriptorImageInfo->sampler = VK_NULL_HANDLE;
    PositionInputDescriptorImageInfo->imageView = State->AttachmentImages.Position.View;
    PositionInputDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet *PositionInputImageWrite = ctk::push(&WriteDescriptorSets);
    PositionInputImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    PositionInputImageWrite->dstSet = InputAttachmentsDS->Instances[0];
    PositionInputImageWrite->dstBinding = 1;
    PositionInputImageWrite->dstArrayElement = 0;
    PositionInputImageWrite->descriptorCount = 1;
    PositionInputImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    PositionInputImageWrite->pImageInfo = PositionInputDescriptorImageInfo;

    VkDescriptorImageInfo *NormalInputDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
    NormalInputDescriptorImageInfo->sampler = VK_NULL_HANDLE;
    NormalInputDescriptorImageInfo->imageView = State->AttachmentImages.Normal.View;
    NormalInputDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet *NormalInputImageWrite = ctk::push(&WriteDescriptorSets);
    NormalInputImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    NormalInputImageWrite->dstSet = InputAttachmentsDS->Instances[0];
    NormalInputImageWrite->dstBinding = 2;
    NormalInputImageWrite->dstArrayElement = 0;
    NormalInputImageWrite->descriptorCount = 1;
    NormalInputImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    NormalInputImageWrite->pImageInfo = NormalInputDescriptorImageInfo;

    // Textures
    CTK_ITERATE(Assets->Textures.Count) {
        vtk::texture *Texture = Assets->Textures.Values + IterationIndex;

        VkDescriptorImageInfo *TextureDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
        TextureDescriptorImageInfo->sampler = Texture->Sampler;
        TextureDescriptorImageInfo->imageView = Texture->Image.View;
        TextureDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vtk::descriptor_set *TexturesDS = ctk::at(&State->DescriptorSets.Textures, Assets->Textures.Keys[IterationIndex]);
        VkWriteDescriptorSet *TextureImageWrite = ctk::push(&WriteDescriptorSets);
        TextureImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        TextureImageWrite->dstSet = TexturesDS->Instances[0];
        TextureImageWrite->dstBinding = 0;
        TextureImageWrite->dstArrayElement = 0;
        TextureImageWrite->descriptorCount = 1;
        TextureImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        TextureImageWrite->pImageInfo = TextureDescriptorImageInfo;
    }

    vkUpdateDescriptorSets(Device->Logical, WriteDescriptorSets.Count, WriteDescriptorSets.Data, 0, NULL);

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////
    vtk::graphics_pipeline_info UnlitColorGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&UnlitColorGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "unlit_color_vert"));
    ctk::push(&UnlitColorGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "unlit_color_frag"));
    ctk::push(&UnlitColorGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.EntityMatrixes);
    ctk::push(&UnlitColorGPInfo.PushConstantRanges, { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ctk::vec4<f32>) });
    ctk::push(&UnlitColorGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    UnlitColorGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&UnlitColorGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&UnlitColorGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&UnlitColorGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    UnlitColorGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    UnlitColorGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    UnlitColorGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    State->GraphicsPipelines.UnlitColor = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, 0, &UnlitColorGPInfo);

    vtk::graphics_pipeline_info DeferredGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&DeferredGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_deferred_vert"));
    ctk::push(&DeferredGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_deferred_frag"));
    ctk::push(&DeferredGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.EntityMatrixes);
    ctk::push(&DeferredGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.Textures);
    ctk::push(&DeferredGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    ctk::push(&DeferredGPInfo.VertexInputs, { 0, 1, State->VertexAttributeIndexes.Normal });
    ctk::push(&DeferredGPInfo.VertexInputs, { 0, 2, State->VertexAttributeIndexes.UV });
    DeferredGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&DeferredGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&DeferredGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&DeferredGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    ctk::push(&DeferredGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    ctk::push(&DeferredGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    DeferredGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    DeferredGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    DeferredGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    State->GraphicsPipelines.Deferred = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, 1, &DeferredGPInfo);

    vtk::graphics_pipeline_info LightingGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&LightingGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_lighting_vert"));
    ctk::push(&LightingGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_lighting_frag"));
    ctk::push(&LightingGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.InputAttachments);
    ctk::push(&LightingGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.Lights);
    ctk::push(&LightingGPInfo.PushConstantRanges, { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(s32) });
    ctk::push(&LightingGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    LightingGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&LightingGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&LightingGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&LightingGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    State->GraphicsPipelines.Lighting = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, 2, &LightingGPInfo);
}

static void create_scene(state *State, assets *Assets, vulkan_instance *VulkanInstance) {
    scene *Scene = &State->Scene;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    ctk::data SceneData = ctk::load_data("assets/scenes/lighting_test.ctkd");

    // Camera
    ctk::data *CameraData = ctk::at(&SceneData, "camera");
    Scene->Camera.Transform = load_transform(ctk::at(CameraData, "transform"));
    Scene->Camera.FieldOfView = ctk::to_f32(CameraData, "field_of_view");
    Scene->Camera.Aspect = Swapchain->Extent.width / (f32)Swapchain->Extent.height;
    Scene->Camera.ZNear = ctk::to_f32(CameraData, "z_near");
    Scene->Camera.ZFar = ctk::to_f32(CameraData, "z_far");

    // EntityMatrixes
    ctk::data *EntityMap = ctk::at(&SceneData, "entities");
    for(u32 EntityIndex = 0; EntityIndex < EntityMap->Children.Count; ++EntityIndex) {
        ctk::data *EntityData = ctk::at(EntityMap, EntityIndex);
        entity *Entity = push_entity(Scene, EntityData->Key.Data);
        Entity->Transform = load_transform(ctk::at(EntityData, "transform"));
        Entity->TextureDS = ctk::at(&State->DescriptorSets.Textures, ctk::to_cstr(EntityData, "texture"));
        Entity->Mesh = ctk::at(&Assets->Meshes, ctk::to_cstr(EntityData, "mesh"));
    }

    // Lights
    ctk::data *LightArray = ctk::at(&SceneData, "lights");
    for(u32 LightIndex = 0; LightIndex < LightArray->Children.Count; ++LightIndex) {
        ctk::data *LightData = ctk::at(LightArray, LightIndex);
        light *Light = push_light(Scene);
        Light->Position = load_vec3(ctk::at(LightData, "position"));
        Light->Range = ctk::to_f32(LightData, "range");
        Light->Color = load_vec4(ctk::at(LightData, "color"));
    }

    // Cleanup
    ctk::_free(&SceneData);
}

static state *create_state(vulkan_instance *VulkanInstance, assets *Assets) {
    auto State = ctk::allocate<state>();
    *State = {};
    State->LightMode = state::LIGHT_MODE_NORMAL;
    create_vulkan_state(State, VulkanInstance, Assets);
    create_scene(State, Assets, VulkanInstance);
    return State;
}

static void record_render_command_buffers(state *State, vulkan_instance *VulkanInstance, assets *Assets, u32 SwapchainImageIndex) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    scene *Scene = &State->Scene;
    vtk::render_pass *RenderPass = &State->RenderPass;

    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = Swapchain->Extent;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = 0;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

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

    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        // Direct Stage
        {
            // Draw Light Diamonds
            vtk::graphics_pipeline *UnlitColorGP = &State->GraphicsPipelines.UnlitColor;
            mesh *LightDiamond = ctk::at(&Assets->Meshes, "light_diamond");
            vtk::descriptor_set *DescriptorSets[] = { &State->DescriptorSets.LightMatrixes };
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UnlitColorGP->Handle);

            // Point Lights
            for(u32 LightIndex = 0; LightIndex < Scene->Lights.Count; ++LightIndex) {
                vtk::bind_descriptor_sets(CommandBuffer, UnlitColorGP->Layout, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets),
                                          SwapchainImageIndex, LightIndex);
                vkCmdPushConstants(CommandBuffer, UnlitColorGP->Layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(ctk::vec3<f32>), &Scene->Lights[LightIndex].Color);
                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &LightDiamond->VertexRegion.Buffer->Handle,
                                       &LightDiamond->VertexRegion.Offset);
                vkCmdBindIndexBuffer(CommandBuffer, LightDiamond->IndexRegion.Buffer->Handle, LightDiamond->IndexRegion.Offset,
                                     VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(CommandBuffer, LightDiamond->Indexes.Count, 1, 0, 0, 0);
            }
        }

        vkCmdNextSubpass(CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

        // Deferred Stage
        {
            vtk::graphics_pipeline *DeferredGP = &State->GraphicsPipelines.Deferred;
            vtk::descriptor_set *DescriptorSets[] = { &State->DescriptorSets.EntityMatrixes };
            for(u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex) {
                entity *Entity = Scene->Entities.Values + EntityIndex;
                VkDescriptorSet *TextureDS = Entity->TextureDS->Instances + 0;
                mesh *Mesh = Entity->Mesh;

                vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DeferredGP->Handle);
                vtk::bind_descriptor_sets(CommandBuffer, DeferredGP->Layout, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets),
                                          SwapchainImageIndex, EntityIndex);
                vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DeferredGP->Layout,
                                        1, 1, TextureDS,
                                        0, NULL);
                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
                vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
            }
        }

        vkCmdNextSubpass(CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

        // Lighting Stage
        {
            vtk::graphics_pipeline *LightingGP = &State->GraphicsPipelines.Lighting;
            mesh *FullscreenPlane = ctk::at(&Assets->Meshes, "fullscreen_plane");
            VkDescriptorSet DescriptorSets[] = {
                State->DescriptorSets.InputAttachments.Instances[0],
                State->DescriptorSets.Lights.Instances[SwapchainImageIndex]
            };

            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingGP->Handle);
            vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingGP->Layout,
                                    0, CTK_ARRAY_COUNT(DescriptorSets), DescriptorSets,
                                    0, NULL);
            vkCmdPushConstants(CommandBuffer, LightingGP->Layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(s32), &State->LightMode);
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &FullscreenPlane->VertexRegion.Buffer->Handle,
                                   &FullscreenPlane->VertexRegion.Offset);
            vkCmdBindIndexBuffer(CommandBuffer, FullscreenPlane->IndexRegion.Buffer->Handle, FullscreenPlane->IndexRegion.Offset,
                                 VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(CommandBuffer, FullscreenPlane->Indexes.Count, 1, 0, 0, 0);
        }
    vkCmdEndRenderPass(CommandBuffer);
    vtk::validate_vk_result(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
}

static void update_scene(VkDevice LogicalDevice, state *State, input_state *InputState, u32 SwapchainImageIndex) {
    scene *Scene = &State->Scene;
    glm::mat4 ViewProjectionMatrix = view_projection_matrix(&Scene->Camera);
    update_entity_matrixes(LogicalDevice, State->UniformBuffers.EntityMatrixes.Regions + SwapchainImageIndex, Scene, ViewProjectionMatrix);
    update_light_matrixes(LogicalDevice, State->UniformBuffers.LightMatrixes.Regions + SwapchainImageIndex, Scene, ViewProjectionMatrix);
    update_lights(LogicalDevice, State->UniformBuffers.Lights.Regions + SwapchainImageIndex, Scene);
}

static void test_main() {
    input_state InputState = {};
    window *Window = create_window(&InputState);
    vulkan_instance *VulkanInstance = create_vulkan_instance(Window);
    assets *Assets = create_assets(VulkanInstance);
    state *State = create_state(VulkanInstance, Assets);
    while(!glfwWindowShouldClose(Window->Handle)) {
        // Check if window should close.
        glfwPollEvents();
        if(InputState.KeyDown[GLFW_KEY_ESCAPE]) break;

        // Process input.
        update_input_state(&InputState, Window->Handle);
        camera_controls(&State->Scene.Camera.Transform, &InputState);

        // Light mode controls.
             if(InputState.KeyDown[GLFW_KEY_F1]) State->LightMode = state::LIGHT_MODE_COMPOSITE;
        else if(InputState.KeyDown[GLFW_KEY_F2]) State->LightMode = state::LIGHT_MODE_ALBEDO;
        else if(InputState.KeyDown[GLFW_KEY_F3]) State->LightMode = state::LIGHT_MODE_POSITION;
        else if(InputState.KeyDown[GLFW_KEY_F4]) State->LightMode = state::LIGHT_MODE_NORMAL;

        // Process frame.
        u32 SwapchainImageIndex = aquire_next_swapchain_image_index(VulkanInstance);
        record_render_command_buffers(State, VulkanInstance, Assets, SwapchainImageIndex);
        update_scene(VulkanInstance->Device.Logical, State, &InputState, SwapchainImageIndex);
        synchronize_current_frame(VulkanInstance, SwapchainImageIndex);
        submit_render_pass(VulkanInstance, &State->RenderPass, SwapchainImageIndex);
        cycle_frame(VulkanInstance);
        Sleep(1);
    }
}
