#pragma once

#include "gfx/tests/shared.h"

struct state {
    static const u32 DEPTH_PEEL_LAYER_COUNT = 24;
    static const u32 DEPTH_PEEL_LAYER_SUBPASS_BASE_INDEX = 2;
    scene Scene;
    vtk::uniform_buffer EntityMatrixesBuffer;
    vtk::render_pass RenderPass;
    VkDescriptorPool DescriptorPool;
    vtk::vertex_layout VertexLayout;
    struct {
        u32 Position;
        u32 Normal;
        u32 UV;
    } VertexAttributeIndexes;
    struct {
        vtk::image PeelDepth[2];
        vtk::image PeelColor;
    } AttachmentImages;
    struct {
        VkDescriptorSetLayout EntityMatrixesBuffer;
        VkDescriptorSetLayout PeelDepthImage;
        VkDescriptorSetLayout PeelColorImage;
    } DescriptorSetLayouts;
    struct {
        vtk::descriptor_set EntityMatrixesBuffer;
        vtk::descriptor_set PeelDepthImage[2];
        vtk::descriptor_set PeelColorImage;
    } DescriptorSets;
    struct {
        vtk::graphics_pipeline FirstPeel;
        vtk::graphics_pipeline FirstBlend;
        vtk::graphics_pipeline Blend[DEPTH_PEEL_LAYER_COUNT];
        vtk::graphics_pipeline Peel[DEPTH_PEEL_LAYER_COUNT];
    } GraphicsPipelines;
};

static void create_vulkan_state(state *State, vulkan_instance *VulkanInstance, assets *Assets) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    ////////////////////////////////////////////////////////////
    /// Buffers
    ////////////////////////////////////////////////////////////
    State->EntityMatrixesBuffer = vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, scene::MAX_ENTITIES, sizeof(matrix_ubo),
                                                     VulkanInstance->Swapchain.Images.Count);

    ////////////////////////////////////////////////////////////
    /// Vertex Layout
    ////////////////////////////////////////////////////////////
    State->VertexAttributeIndexes.Position = vtk::push_vertex_attribute(&State->VertexLayout, 3);
    State->VertexAttributeIndexes.Normal = vtk::push_vertex_attribute(&State->VertexLayout, 3);
    State->VertexAttributeIndexes.UV = vtk::push_vertex_attribute(&State->VertexLayout, 2);

    ////////////////////////////////////////////////////////////
    /// Attachment Images
    ////////////////////////////////////////////////////////////
    vtk::image_info PeelDepthImageInfo = {};
    PeelDepthImageInfo.Width = Swapchain->Extent.width;
    PeelDepthImageInfo.Height = Swapchain->Extent.height;
    PeelDepthImageInfo.Format = vtk::find_depth_image_format(Device->Physical);
    PeelDepthImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    PeelDepthImageInfo.UsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    PeelDepthImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    PeelDepthImageInfo.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    CTK_ITERATE(2) State->AttachmentImages.PeelDepth[IterationIndex] = vtk::create_image(Device, &PeelDepthImageInfo);

    vtk::image_info PeelColorImageInfo = {};
    PeelColorImageInfo.Width = Swapchain->Extent.width;
    PeelColorImageInfo.Height = Swapchain->Extent.height;
    PeelColorImageInfo.Format = Swapchain->ImageFormat;
    PeelColorImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    PeelColorImageInfo.UsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    PeelColorImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    PeelColorImageInfo.AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    State->AttachmentImages.PeelColor = vtk::create_image(Device, &PeelColorImageInfo);

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass_info RenderPassInfo = {};

    // Attachments
    static const struct {
        u32 PeelDepth[2] = { 0, 1 };
        u32 PeelColor = 2;
        u32 SwapchainImage = 3;
    } ATTACHMENT_INDEXES;

    CTK_ITERATE(2) {
        vtk::attachment *PeelDepthAttachment = ctk::push(&RenderPassInfo.Attachments);
        PeelDepthAttachment->Description.format = PeelDepthImageInfo.Format;
        PeelDepthAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
        PeelDepthAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        PeelDepthAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        PeelDepthAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        PeelDepthAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        PeelDepthAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        PeelDepthAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        PeelDepthAttachment->ClearValue = { 1.0f, 0 };
    }

    vtk::attachment *PeelColorAttachment = ctk::push(&RenderPassInfo.Attachments);
    PeelColorAttachment->Description.format = PeelColorImageInfo.Format;
    PeelColorAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    PeelColorAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    PeelColorAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    PeelColorAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    PeelColorAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    PeelColorAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    PeelColorAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    PeelColorAttachment->ClearValue = { 0, 0, 0, 0 };

    vtk::attachment *SwapchainImageAttachment = ctk::push(&RenderPassInfo.Attachments);
    SwapchainImageAttachment->Description.format = Swapchain->ImageFormat;
    SwapchainImageAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    SwapchainImageAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    SwapchainImageAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    SwapchainImageAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    SwapchainImageAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    SwapchainImageAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    SwapchainImageAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    SwapchainImageAttachment->ClearValue = { 0, 0, 0, 1 };

    // Subpasses
    vtk::subpass *FirstPeelSubpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::set(&FirstPeelSubpass->DepthAttachmentReference, { ATTACHMENT_INDEXES.PeelDepth[0], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
    ctk::push(&FirstPeelSubpass->ColorAttachmentReferences, { ATTACHMENT_INDEXES.PeelColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    vtk::subpass *FirstBlendSubpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::push(&FirstBlendSubpass->PreserveAttachmentIndexes, ATTACHMENT_INDEXES.PeelDepth[0]);
    ctk::push(&FirstBlendSubpass->PreserveAttachmentIndexes, ATTACHMENT_INDEXES.PeelDepth[1]);
    ctk::push(&FirstBlendSubpass->InputAttachmentReferences, { ATTACHMENT_INDEXES.PeelColor, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&FirstBlendSubpass->ColorAttachmentReferences, { ATTACHMENT_INDEXES.SwapchainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    CTK_ITERATE(state::DEPTH_PEEL_LAYER_COUNT) {
        vtk::subpass *PeelSubpass = ctk::push(&RenderPassInfo.Subpasses);
        u32 InputDepthAttachmentIndex;
        u32 OutputDepthAttachmentIndex;
        if((IterationIndex % 2) == 0) {
            InputDepthAttachmentIndex = 0;
            OutputDepthAttachmentIndex = 1;
        } else {
            InputDepthAttachmentIndex = 1;
            OutputDepthAttachmentIndex = 0;
        }
        ctk::push(&PeelSubpass->PreserveAttachmentIndexes, ATTACHMENT_INDEXES.SwapchainImage);
        ctk::push(&PeelSubpass->InputAttachmentReferences, { ATTACHMENT_INDEXES.PeelDepth[InputDepthAttachmentIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        ctk::set(&PeelSubpass->DepthAttachmentReference, { ATTACHMENT_INDEXES.PeelDepth[OutputDepthAttachmentIndex], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
        ctk::push(&PeelSubpass->ColorAttachmentReferences, { ATTACHMENT_INDEXES.PeelColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

        vtk::subpass *BlendSubpass = ctk::push(&RenderPassInfo.Subpasses);
        ctk::push(&BlendSubpass->PreserveAttachmentIndexes, ATTACHMENT_INDEXES.PeelDepth[0]);
        ctk::push(&BlendSubpass->PreserveAttachmentIndexes, ATTACHMENT_INDEXES.PeelDepth[1]);
        ctk::push(&BlendSubpass->InputAttachmentReferences, { ATTACHMENT_INDEXES.PeelColor, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        ctk::push(&BlendSubpass->ColorAttachmentReferences, { ATTACHMENT_INDEXES.SwapchainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    }

    // Subpass Dependencies
    RenderPassInfo.SubpassDependencies.Count = 2 + (state::DEPTH_PEEL_LAYER_COUNT * 2);
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

    CTK_ITERATE(state::DEPTH_PEEL_LAYER_COUNT) {
        u32 DepthPeelLayerSubpassOffsetIndex = state::DEPTH_PEEL_LAYER_SUBPASS_BASE_INDEX + (IterationIndex * 2);
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 0].srcSubpass = DepthPeelLayerSubpassOffsetIndex - 1;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 0].dstSubpass = DepthPeelLayerSubpassOffsetIndex + 0;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 1].srcSubpass = DepthPeelLayerSubpassOffsetIndex + 0;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 1].dstSubpass = DepthPeelLayerSubpassOffsetIndex + 1;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 1].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        RenderPassInfo.SubpassDependencies[DepthPeelLayerSubpassOffsetIndex + 1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }

    // Framebuffer Infos
    CTK_ITERATE(Swapchain->Images.Count) {
        vtk::framebuffer_info *FramebufferInfo = ctk::push(&RenderPassInfo.FramebufferInfos);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.PeelDepth[0].View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.PeelDepth[1].View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.PeelColor.View);
        ctk::push(&FramebufferInfo->Attachments, Swapchain->Images[IterationIndex].View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    State->RenderPass = vtk::create_render_pass(Device->Logical, VulkanInstance->GraphicsCommandPool, &RenderPassInfo);

    ////////////////////////////////////////////////////////////
    /// Descriptor Sets
    ////////////////////////////////////////////////////////////
    vtk::descriptor_set *EntityMatrixesBufferDS = &State->DescriptorSets.EntityMatrixesBuffer;
    vtk::descriptor_set *PeelDepthImageDS = State->DescriptorSets.PeelDepthImage;
    vtk::descriptor_set *PeelColorImageDS = &State->DescriptorSets.PeelColorImage;

    // Pool
    static const u32 PEEL_DEPTH_ATTACHMENT_COUNT = 2;
    static const u32 PEEL_COLOR_ATTACHMENT_COUNT = 1;
    ctk::sarray<VkDescriptorPoolSize, 4> DescriptorPoolSizes = {};
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, Swapchain->Images.Count });
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, PEEL_DEPTH_ATTACHMENT_COUNT + PEEL_COLOR_ATTACHMENT_COUNT });
    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = {};
    DescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolCreateInfo.flags = 0;
    DescriptorPoolCreateInfo.maxSets = 16;
    DescriptorPoolCreateInfo.poolSizeCount = DescriptorPoolSizes.Count;
    DescriptorPoolCreateInfo.pPoolSizes = DescriptorPoolSizes.Data;
    vtk::validate_vk_result(vkCreateDescriptorPool(Device->Logical, &DescriptorPoolCreateInfo, NULL, &State->DescriptorPool),
                            "vkCreateDescriptorPool", "failed to create descriptor pool");

    // EntityMatrixesBuffer
    {
        // Layout
        ctk::sarray<VkDescriptorSetLayoutBinding, 4> LayoutBindings = {};
        ctk::push(&LayoutBindings, { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT });

        VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = {};
        DescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        DescriptorSetLayoutCreateInfo.bindingCount = LayoutBindings.Count;
        DescriptorSetLayoutCreateInfo.pBindings = LayoutBindings.Data;
        vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &DescriptorSetLayoutCreateInfo, NULL,
                                                            &State->DescriptorSetLayouts.EntityMatrixesBuffer),
                                "vkCreateDescriptorSetLayout", "error creating descriptor set layout");

        // Initialization
        EntityMatrixesBufferDS->Instances.Count = Swapchain->Images.Count;
        ctk::push(&EntityMatrixesBufferDS->DynamicOffsets, State->EntityMatrixesBuffer.ElementSize);

        // Allocation
        ctk::sarray<VkDescriptorSetLayout, 4> DuplicateLayouts = {};
        CTK_ITERATE(EntityMatrixesBufferDS->Instances.Count) ctk::push(&DuplicateLayouts, State->DescriptorSetLayouts.EntityMatrixesBuffer);

        VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = {};
        DescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        DescriptorSetAllocateInfo.descriptorPool = State->DescriptorPool;
        DescriptorSetAllocateInfo.descriptorSetCount = EntityMatrixesBufferDS->Instances.Count;
        DescriptorSetAllocateInfo.pSetLayouts = DuplicateLayouts.Data;
        vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &DescriptorSetAllocateInfo, EntityMatrixesBufferDS->Instances.Data),
                                "vkAllocateDescriptorSets", "failed to allocate descriptor sets");
    }

    // PeelDepthImages
    {
        // Layouts
        ctk::sarray<VkDescriptorSetLayoutBinding, 4> LayoutBindings = {};
        ctk::push(&LayoutBindings, { 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT });

        VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = {};
        DescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        DescriptorSetLayoutCreateInfo.bindingCount = LayoutBindings.Count;
        DescriptorSetLayoutCreateInfo.pBindings = LayoutBindings.Data;
        vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &DescriptorSetLayoutCreateInfo, NULL,
                                                            &State->DescriptorSetLayouts.PeelDepthImage),
                                "vkCreateDescriptorSetLayout", "error creating descriptor set layout");

        CTK_ITERATE(2) {
            // Initialization
            PeelDepthImageDS[IterationIndex].Instances.Count = 1;

            // Allocation
            VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = {};
            DescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            DescriptorSetAllocateInfo.descriptorPool = State->DescriptorPool;
            DescriptorSetAllocateInfo.descriptorSetCount = PeelDepthImageDS[IterationIndex].Instances.Count;
            DescriptorSetAllocateInfo.pSetLayouts = &State->DescriptorSetLayouts.PeelDepthImage;
            vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &DescriptorSetAllocateInfo,
                                                             PeelDepthImageDS[IterationIndex].Instances.Data),
                                    "vkAllocateDescriptorSets", "failed to allocate descriptor sets");
        }
    }

    // PeelColorImage
    {
        // Layouts
        ctk::sarray<VkDescriptorSetLayoutBinding, 4> LayoutBindings = {};
        ctk::push(&LayoutBindings, { 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT });

        VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = {};
        DescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        DescriptorSetLayoutCreateInfo.bindingCount = LayoutBindings.Count;
        DescriptorSetLayoutCreateInfo.pBindings = LayoutBindings.Data;
        vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &DescriptorSetLayoutCreateInfo, NULL,
                                                            &State->DescriptorSetLayouts.PeelColorImage),
                                "vkCreateDescriptorSetLayout", "error creating descriptor set layout");

        // Initialization
        PeelColorImageDS->Instances.Count = 1;

        // Allocation
        VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = {};
        DescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        DescriptorSetAllocateInfo.descriptorPool = State->DescriptorPool;
        DescriptorSetAllocateInfo.descriptorSetCount = PeelColorImageDS->Instances.Count;
        DescriptorSetAllocateInfo.pSetLayouts = &State->DescriptorSetLayouts.PeelColorImage;
        vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &DescriptorSetAllocateInfo, PeelColorImageDS->Instances.Data),
                                "vkAllocateDescriptorSets", "failed to allocate descriptor sets");
    }

    // Updates
    ctk::sarray<VkDescriptorBufferInfo, 16> DescriptorBufferInfos = {};
    ctk::sarray<VkDescriptorImageInfo, 16> DescriptorImageInfos = {};
    ctk::sarray<VkWriteDescriptorSet, 16> WriteDescriptorSets = {};

    // EntityMatrixesBuffer
    CTK_ITERATE(EntityMatrixesBufferDS->Instances.Count) {
        VkDescriptorSet DescriptorSet = EntityMatrixesBufferDS->Instances[IterationIndex];
        vtk::region *EntityMatrixesBufferRegion = State->EntityMatrixesBuffer.Regions + IterationIndex;

        VkDescriptorBufferInfo *EntityDescriptorBufferInfo = ctk::push(&DescriptorBufferInfos);
        EntityDescriptorBufferInfo->buffer = EntityMatrixesBufferRegion->Buffer->Handle;
        EntityDescriptorBufferInfo->offset = EntityMatrixesBufferRegion->Offset;
        EntityDescriptorBufferInfo->range = EntityMatrixesBufferRegion->Size;

        VkWriteDescriptorSet *EntityMatrixesBufferWrite = ctk::push(&WriteDescriptorSets);
        EntityMatrixesBufferWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        EntityMatrixesBufferWrite->dstSet = DescriptorSet;
        EntityMatrixesBufferWrite->dstBinding = 0;
        EntityMatrixesBufferWrite->dstArrayElement = 0;
        EntityMatrixesBufferWrite->descriptorCount = 1;
        EntityMatrixesBufferWrite->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        EntityMatrixesBufferWrite->pBufferInfo = EntityDescriptorBufferInfo;
    }

    // PeelDepthImages
    CTK_ITERATE(2) {
        VkDescriptorImageInfo *PeelDepthDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
        PeelDepthDescriptorImageInfo->sampler = VK_NULL_HANDLE;
        PeelDepthDescriptorImageInfo->imageView = State->AttachmentImages.PeelDepth[IterationIndex].View;
        PeelDepthDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet *PeelDepthImageWrite = ctk::push(&WriteDescriptorSets);
        PeelDepthImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        PeelDepthImageWrite->dstSet = PeelDepthImageDS[IterationIndex].Instances[0];
        PeelDepthImageWrite->dstBinding = 0;
        PeelDepthImageWrite->dstArrayElement = 0;
        PeelDepthImageWrite->descriptorCount = 1;
        PeelDepthImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        PeelDepthImageWrite->pImageInfo = PeelDepthDescriptorImageInfo;
    }

    // PeelColorImage
    VkDescriptorImageInfo *PeelColorDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
    PeelColorDescriptorImageInfo->sampler = VK_NULL_HANDLE;
    PeelColorDescriptorImageInfo->imageView = State->AttachmentImages.PeelColor.View;
    PeelColorDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet *PeelColorImageWrite = ctk::push(&WriteDescriptorSets);
    PeelColorImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    PeelColorImageWrite->dstSet = PeelColorImageDS->Instances[0];
    PeelColorImageWrite->dstBinding = 0;
    PeelColorImageWrite->dstArrayElement = 0;
    PeelColorImageWrite->descriptorCount = 1;
    PeelColorImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    PeelColorImageWrite->pImageInfo = PeelColorDescriptorImageInfo;

    vkUpdateDescriptorSets(Device->Logical, WriteDescriptorSets.Count, WriteDescriptorSets.Data, 0, NULL);

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////

    // Info
    vtk::graphics_pipeline_info FirstPeelGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&FirstPeelGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_first_peel_vert"));
    ctk::push(&FirstPeelGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_first_peel_frag"));
    ctk::push(&FirstPeelGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.EntityMatrixesBuffer);
    ctk::push(&FirstPeelGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    FirstPeelGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&FirstPeelGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&FirstPeelGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&FirstPeelGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    // ctk::push(&FirstPeelGPInfo.ColorBlendAttachmentStates, {
    //     VK_TRUE,
    //     VK_BLEND_FACTOR_SRC_ALPHA,
    //     VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    //     VK_BLEND_OP_ADD,
    //     VK_BLEND_FACTOR_ONE,
    //     VK_BLEND_FACTOR_ZERO,
    //     VK_BLEND_OP_ADD,
    //     VTK_COLOR_COMPONENT_RGBA,
    // });
    FirstPeelGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    FirstPeelGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    FirstPeelGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    vtk::graphics_pipeline_info BlendGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&BlendGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_blend_vert"));
    ctk::push(&BlendGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_blend_frag"));
    ctk::push(&BlendGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.PeelColorImage);
    ctk::push(&BlendGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    BlendGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&BlendGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&BlendGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&BlendGPInfo.ColorBlendAttachmentStates, {
        VK_TRUE,
        VK_BLEND_FACTOR_DST_ALPHA,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_OP_ADD,
        VTK_COLOR_COMPONENT_RGBA,
    });

    vtk::graphics_pipeline_info PeelGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&PeelGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_peel_vert"));
    ctk::push(&PeelGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "depth_peeling_peel_frag"));
    ctk::push(&PeelGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.EntityMatrixesBuffer);
    ctk::push(&PeelGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.PeelDepthImage);
    ctk::push(&PeelGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    PeelGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&PeelGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&PeelGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&PeelGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    PeelGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    PeelGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    PeelGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    // Creation
    State->GraphicsPipelines.FirstPeel = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, 0, &FirstPeelGPInfo);
    State->GraphicsPipelines.FirstBlend = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, 1, &BlendGPInfo);
    CTK_ITERATE(state::DEPTH_PEEL_LAYER_COUNT) {
        u32 DepthPeelLayerSubpassOffsetIndex = state::DEPTH_PEEL_LAYER_SUBPASS_BASE_INDEX + (IterationIndex * 2);
        State->GraphicsPipelines.Peel[IterationIndex] =
            vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, DepthPeelLayerSubpassOffsetIndex + 0, &PeelGPInfo);
        State->GraphicsPipelines.Blend[IterationIndex] =
            vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, DepthPeelLayerSubpassOffsetIndex + 1, &BlendGPInfo);
    }
}

static void create_scene(state *State, assets *Assets, vulkan_instance *VulkanInstance) {
    scene *Scene = &State->Scene;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    ctk::data SceneData = ctk::load_data("assets/scenes/empty.ctkd");

    // Camera
    ctk::data *CameraData = ctk::at(&SceneData, "camera");
    Scene->Camera.Transform = load_transform(ctk::at(CameraData, "transform"));
    Scene->Camera.FieldOfView = ctk::to_f32(CameraData, "field_of_view");
    Scene->Camera.Aspect = Swapchain->Extent.width / (f32)Swapchain->Extent.height;
    Scene->Camera.ZNear = ctk::to_f32(CameraData, "z_near");
    Scene->Camera.ZFar = ctk::to_f32(CameraData, "z_far");

    // Create Entities
    for(s32 Z = 0; Z < 8; ++Z)
    for(s32 Y = 0; Y < 1; ++Y)
    for(s32 X = 0; X < 8; ++X) {
        char Name[16] = {};
        sprintf(Name, "cube_%u", ((Y * 8 * 8) + (Z * 8)) + X);
        entity *Cube = push_entity(Scene, Name);
        Cube->Transform.Position = { 1.5f * X, 1.5f * (-Y), 1.5f * Z };
        Cube->Transform.Scale = { 1, 1, 1 };
        Cube->Mesh = ctk::at(&Assets->Meshes, "cube");
    }

    // Cleanup
    ctk::_free(&SceneData);
}

static state *create_state(vulkan_instance *VulkanInstance, assets *Assets) {
    auto State = ctk::allocate<state>();
    *State = {};
    create_vulkan_state(State, VulkanInstance, Assets);
    create_scene(State, Assets, VulkanInstance);
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

        vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            // First Peel Stage
            {
                vtk::graphics_pipeline *FirstPeelGP = &State->GraphicsPipelines.FirstPeel;
                vtk::descriptor_set *DescriptorSets[] = { &State->DescriptorSets.EntityMatrixesBuffer };
                for(u32 EntityIndex = 0; EntityIndex < State->Scene.Entities.Count; ++EntityIndex) {
                    mesh *Mesh = State->Scene.Entities.Values[EntityIndex].Mesh;

                    // Graphics Pipeline
                    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, FirstPeelGP->Handle);

                    // Bind Descriptor Sets
                    vtk::bind_descriptor_sets(CommandBuffer, FirstPeelGP->Layout, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets),
                                              SwapchainImageIndex, EntityIndex);

                    // Vertex/Index Buffers
                    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
                    vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);

                    // Draw
                    vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
                }
            }

            // First Blend Stage
            {
                vtk::graphics_pipeline *FirstBlendGP = &State->GraphicsPipelines.FirstBlend;
                vtk::descriptor_set *DescriptorSets[] = { &State->DescriptorSets.PeelColorImage };
                mesh *FullscreenPlane = ctk::at(&Assets->Meshes, "fullscreen_plane");

                // Increment Subpass
                vkCmdNextSubpass(CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

                // Graphics Pipeline
                vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, FirstBlendGP->Handle);

                // Bind Descriptor Sets
                vtk::bind_descriptor_sets(CommandBuffer, FirstBlendGP->Layout, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets));

                // Vertex/Index Buffers
                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &FullscreenPlane->VertexRegion.Buffer->Handle,
                                       &FullscreenPlane->VertexRegion.Offset);
                vkCmdBindIndexBuffer(CommandBuffer, FullscreenPlane->IndexRegion.Buffer->Handle, FullscreenPlane->IndexRegion.Offset,
                                     VK_INDEX_TYPE_UINT32);

                // Draw
                vkCmdDrawIndexed(CommandBuffer, FullscreenPlane->Indexes.Count, 1, 0, 0, 0);
            }

            CTK_ITERATE(state::DEPTH_PEEL_LAYER_COUNT) {
                // Peel Stage
                {
                    vtk::graphics_pipeline *PeelGP = State->GraphicsPipelines.Peel + IterationIndex;
                    vtk::descriptor_set *DescriptorSets[] = { &State->DescriptorSets.EntityMatrixesBuffer };

                    // Increment Subpass
                    vkCmdNextSubpass(CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

                    // Clear attachments before rendering.
                    VkClearAttachment AttachmentClears[2] = {};
                    AttachmentClears[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    AttachmentClears[0].clearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
                    AttachmentClears[0].colorAttachment = 0;
                    AttachmentClears[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                    AttachmentClears[1].clearValue.depthStencil.depth = 1.0f;
                    AttachmentClears[1].clearValue.depthStencil.stencil = 0;
                    VkClearRect AttachmentClearRect = {};
                    AttachmentClearRect.baseArrayLayer = 0;
                    AttachmentClearRect.layerCount = 1;
                    AttachmentClearRect.rect.extent = Swapchain->Extent;
                    AttachmentClearRect.rect.offset = { 0, 0 };
                    vkCmdClearAttachments(CommandBuffer, 2, AttachmentClears, 1, &AttachmentClearRect);

                    // Bind peel depth image input attachment descriptor set.
                    u32 InputAttachmentDescriptorSetIndex = (IterationIndex % 2) == 0 ? 0 : 1;
                    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PeelGP->Layout,
                                            1, 1, State->DescriptorSets.PeelDepthImage[InputAttachmentDescriptorSetIndex].Instances + 0,
                                            0, NULL);

                    for(u32 EntityIndex = 0; EntityIndex < State->Scene.Entities.Count; ++EntityIndex) {
                        mesh *Mesh = State->Scene.Entities.Values[EntityIndex].Mesh;

                        // Graphics Pipeline
                        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PeelGP->Handle);

                        // Bind Descriptor Sets
                        vtk::bind_descriptor_sets(CommandBuffer, PeelGP->Layout, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets),
                                                  SwapchainImageIndex, EntityIndex);

                        // Vertex/Index Buffers
                        vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
                        vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset,
                                             VK_INDEX_TYPE_UINT32);

                        // Draw
                        vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
                    }
                }

                // Blend Stage
                {
                    vtk::graphics_pipeline *BlendGP = State->GraphicsPipelines.Blend + IterationIndex;
                    vtk::descriptor_set *DescriptorSets[] = { &State->DescriptorSets.PeelColorImage };
                    mesh *FullscreenPlane = ctk::at(&Assets->Meshes, "fullscreen_plane");

                    // Increment Subpass
                    vkCmdNextSubpass(CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

                    // Graphics Pipeline
                    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, BlendGP->Handle);

                    // Bind Descriptor Sets
                    vtk::bind_descriptor_sets(CommandBuffer, BlendGP->Layout, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets));

                    // Vertex/Index Buffers
                    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &FullscreenPlane->VertexRegion.Buffer->Handle,
                                           &FullscreenPlane->VertexRegion.Offset);
                    vkCmdBindIndexBuffer(CommandBuffer, FullscreenPlane->IndexRegion.Buffer->Handle, FullscreenPlane->IndexRegion.Offset,
                                         VK_INDEX_TYPE_UINT32);

                    // Draw
                    vkCmdDrawIndexed(CommandBuffer, FullscreenPlane->Indexes.Count, 1, 0, 0, 0);
                }
            }
        vkCmdEndRenderPass(CommandBuffer);
        vtk::validate_vk_result(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
    }
}

static void update_scene(VkDevice LogicalDevice, state *State, input_state *InputState, u32 SwapchainImageIndex) {
    scene *Scene = &State->Scene;
    glm::mat4 ViewProjectionMatrix = view_projection_matrix(&Scene->Camera);
    update_entity_matrixes(LogicalDevice, State->EntityMatrixesBuffer.Regions + SwapchainImageIndex, Scene, ViewProjectionMatrix);
}

static void test_main() {
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

        // Process input.
        update_input_state(&InputState, Window->Handle);
        camera_controls(&State->Scene.Camera.Transform, &InputState);

        // Process frame.
        u32 SwapchainImageIndex = aquire_next_swapchain_image_index(VulkanInstance);
        update_scene(VulkanInstance->Device.Logical, State, &InputState, SwapchainImageIndex);
        synchronize_current_frame(VulkanInstance, SwapchainImageIndex);
        submit_render_pass(VulkanInstance, &State->RenderPass, SwapchainImageIndex);
        cycle_frame(VulkanInstance);
        Sleep(1);
    }
}
