#pragma once

#include "gfx/tests/shared.h"

static const u32 ATTENUATION_VALUE_COUNT = 12;
static const ctk::vec2<f32> ATTENUATION_VALUES[ATTENUATION_VALUE_COUNT] = {
    { 0.7f,    1.8f      },
    { 0.35f,   0.44f     },
    { 0.22f,   0.20f     },
    { 0.14f,   0.07f     },
    { 0.09f,   0.032f    },
    { 0.07f,   0.017f    },
    { 0.045f,  0.0075f   },
    { 0.027f,  0.0028f   },
    { 0.022f,  0.0019f   },
    { 0.014f,  0.0007f   },
    { 0.007f,  0.0002f   },
    { 0.0014f, 0.000007f },
};

struct matrix_ubo {
    alignas(16) glm::mat4 ModelMatrix;
    alignas(16) glm::mat4 ModelViewProjectionMatrix;
};

struct light {
    ctk::vec4<f32> Color;
    ctk::vec3<f32> Position;
    f32 Linear;
    f32 Quadratic;
    f32 Intensity;
    char Pad[8];
};

struct entity {
    transform Transform;
    vtk::descriptor_set* TextureDS;
    mesh* Mesh;
    u32 MaterialIndex;
};

struct scene {
    static const u32 MAX_ENTITIES = 1024;
    static const u32 MAX_LIGHTS = 16;
    camera Camera;
    ctk::smap<entity, MAX_ENTITIES> Entities;
    ctk::sarray<light, MAX_LIGHTS> Lights;
    ctk::sarray<u32, MAX_LIGHTS> LightAttenuationIndexes;
    ctk::sarray<transform, MAX_LIGHTS> LightTransforms;
    ctk::sarray<matrix_ubo, MAX_ENTITIES> EntityMatrixUBOs;
    ctk::sarray<matrix_ubo, MAX_LIGHTS> LightMatrixUBOs;
};

struct lighting_push_constants {
    ctk::vec3<f32> ViewPosition;
    s32 Mode;
};

struct material {
    u32 ShineExponent;
    char Pad[12];
};

struct control_state {
    enum {
        MODE_ENTITY,
        MODE_LIGHT,
    } Mode;
    enum {
        TRANSFORM_TRANSLATE,
        TRANSFORM_ROTATE,
        TRANSFORM_SCALE,
    } TransformMode;
    u32 EntityIndex;
    u32 LightIndex;
};

struct state {
    static const u32 MAX_MATERIALS = 16;
    scene Scene;
    control_state ControlState;
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
        vtk::uniform_buffer Materials;
    } UniformBuffers;
    struct {
        vtk::image Albedo;
        vtk::image Position;
        vtk::image Normal;
        vtk::image Depth;
        vtk::image MaterialIndex;
    } AttachmentImages;
    struct {
        VkDescriptorSetLayout EntityMatrixes;
        VkDescriptorSetLayout Lights;
        VkDescriptorSetLayout InputAttachments;
        VkDescriptorSetLayout Textures;
        VkDescriptorSetLayout Materials;
    } DescriptorSetLayouts;
    struct {
        vtk::descriptor_set EntityMatrixes;
        vtk::descriptor_set LightMatrixes;
        vtk::descriptor_set Lights;
        vtk::descriptor_set InputAttachments;
        ctk::smap<vtk::descriptor_set, 16> Textures;
        vtk::descriptor_set Materials;
    } DescriptorSets;
    struct {
        vtk::graphics_pipeline Deferred;
        vtk::graphics_pipeline Lighting;
        vtk::graphics_pipeline UnlitColor;
    } GraphicsPipelines;
    struct {
        lighting_push_constants Lighting;
    } PushConstants;
    ctk::smap<material, MAX_MATERIALS> Materials;
};

static entity* push_entity(scene* Scene, cstr Name) {
    ctk::push(&Scene->EntityMatrixUBOs);
    return ctk::push(&Scene->Entities, Name);
}

static light* push_light(scene* Scene, u32 AttenuationIndex, transform** LightTransform) {
    ctk::push(&Scene->LightMatrixUBOs);
    ctk::push(&Scene->LightAttenuationIndexes, AttenuationIndex);
    *LightTransform = ctk::push(&Scene->LightTransforms, default_transform());
    return ctk::push(&Scene->Lights);
}

static void create_test_assets(state* State) {
    ctk::push(&State->Materials, "test", { 32 });
    ctk::push(&State->Materials, "test2", { 256 });
}

static void create_vulkan_state(state* State, vulkan_instance* VulkanInstance, assets* Assets) {
    vtk::device* Device = &VulkanInstance->Device;
    vtk::swapchain* Swapchain = &VulkanInstance->Swapchain;

    ////////////////////////////////////////////////////////////
    /// Vertex Layout
    ////////////////////////////////////////////////////////////
    State->VertexAttributeIndexes.Position = vtk::push_vertex_attribute(&State->VertexLayout, 3);
    State->VertexAttributeIndexes.Normal = vtk::push_vertex_attribute(&State->VertexLayout, 3);
    State->VertexAttributeIndexes.UV = vtk::push_vertex_attribute(&State->VertexLayout, 2);

    ////////////////////////////////////////////////////////////
    /// Buffers
    ////////////////////////////////////////////////////////////
    static const u32 UNIFORM_BUFFER_ARRAY_PADDING = 8;
    State->UniformBuffers.EntityMatrixes = vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, &VulkanInstance->Device,
                                                                      scene::MAX_ENTITIES, sizeof(matrix_ubo), Swapchain->Images.Count);
    State->UniformBuffers.LightMatrixes = vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, &VulkanInstance->Device,
                                                                     scene::MAX_LIGHTS, sizeof(matrix_ubo), Swapchain->Images.Count);
    State->UniformBuffers.Lights = vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, &VulkanInstance->Device,
                                                              1, sizeof(State->Scene.Lights) + UNIFORM_BUFFER_ARRAY_PADDING, Swapchain->Images.Count);
    State->UniformBuffers.Materials = vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, &VulkanInstance->Device,
                                                                 state::MAX_MATERIALS, sizeof(material), Swapchain->Images.Count);

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

    vtk::image_info MaterialIndexImageInfo = {};
    MaterialIndexImageInfo.Width = Swapchain->Extent.width;
    MaterialIndexImageInfo.Height = Swapchain->Extent.height;
    MaterialIndexImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    MaterialIndexImageInfo.UsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    MaterialIndexImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    MaterialIndexImageInfo.AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    MaterialIndexImageInfo.Format = VK_FORMAT_R8_UINT;
    State->AttachmentImages.MaterialIndex = vtk::create_image(Device, &MaterialIndexImageInfo);

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass_info RenderPassInfo = {};

    // Attachments
    vtk::attachment* AlbedoAttachment = ctk::push(&RenderPassInfo.Attachments);
    AlbedoAttachment->Description.format = VK_FORMAT_R8G8B8A8_UNORM;
    AlbedoAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    AlbedoAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    AlbedoAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    AlbedoAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    AlbedoAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    AlbedoAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    AlbedoAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    AlbedoAttachment->ClearValue = { 0, 0, 0, 1 };

    vtk::attachment* PositionAttachment = ctk::push(&RenderPassInfo.Attachments);
    PositionAttachment->Description.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    PositionAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    PositionAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    PositionAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    PositionAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    PositionAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    PositionAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    PositionAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    PositionAttachment->ClearValue = { 0, 0, 0, 1 };

    vtk::attachment* NormalAttachment = ctk::push(&RenderPassInfo.Attachments);
    NormalAttachment->Description.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    NormalAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    NormalAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    NormalAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    NormalAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    NormalAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    NormalAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    NormalAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    NormalAttachment->ClearValue = { 0, 0, 0, 1 };

    vtk::attachment* DepthAttachment = ctk::push(&RenderPassInfo.Attachments);
    DepthAttachment->Description.format = DepthImageInfo.Format;
    DepthAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    DepthAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    DepthAttachment->ClearValue = { 1.0f, 0 };

    vtk::attachment* SwapchainAttachment = ctk::push(&RenderPassInfo.Attachments);
    SwapchainAttachment->Description.format = Swapchain->ImageFormat;
    SwapchainAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    SwapchainAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    SwapchainAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    SwapchainAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    SwapchainAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    SwapchainAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    SwapchainAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    SwapchainAttachment->ClearValue = { 0, 0, 0, 1 };

    vtk::attachment* MaterialIndexAttachment = ctk::push(&RenderPassInfo.Attachments);
    MaterialIndexAttachment->Description.format = VK_FORMAT_R8_UINT;
    MaterialIndexAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    MaterialIndexAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    MaterialIndexAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    MaterialIndexAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    MaterialIndexAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    MaterialIndexAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    MaterialIndexAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    MaterialIndexAttachment->ClearValue = { 0 };

    // Subpasses
    vtk::subpass* DeferredSubpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::set(&DeferredSubpass->DepthAttachmentReference, { 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });

    vtk::subpass* LightingSubpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 5, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->ColorAttachmentReferences, { 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    vtk::subpass* DirectSubpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::push(&DirectSubpass->ColorAttachmentReferences, { 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::set(&DirectSubpass->DepthAttachmentReference, { 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });

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
        vtk::framebuffer_info* FramebufferInfo = ctk::push(&RenderPassInfo.FramebufferInfos);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Albedo.View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Position.View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Normal.View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Depth.View);
        ctk::push(&FramebufferInfo->Attachments, Swapchain->Images[IterationIndex].View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.MaterialIndex.View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    State->RenderPass = vtk::create_render_pass(Device->Logical, VulkanInstance->GraphicsCommandPool, &RenderPassInfo);

    ////////////////////////////////////////////////////////////
    /// Descriptor Sets
    ////////////////////////////////////////////////////////////
    vtk::descriptor_set* EntityMatrixesDS = &State->DescriptorSets.EntityMatrixes;
    vtk::descriptor_set* LightMatrixesDS = &State->DescriptorSets.LightMatrixes;
    vtk::descriptor_set* LightsDS = &State->DescriptorSets.Lights;
    vtk::descriptor_set* InputAttachmentsDS = &State->DescriptorSets.InputAttachments;
    vtk::descriptor_set* MaterialsDS = &State->DescriptorSets.Materials;

    // Pool
    ctk::sarray<VkDescriptorPoolSize, 8> DescriptorPoolSizes = {};
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 16 });
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16 });
    ctk::push(&DescriptorPoolSizes, { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 16 });
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
    ctk::push(&InputAttachmentsLayoutBindings, { 3, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT });

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

    // Materials
    ctk::sarray<VkDescriptorSetLayoutBinding, 4> MaterialsLayoutBindings = {};
    ctk::push(&MaterialsLayoutBindings, { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT });

    VkDescriptorSetLayoutCreateInfo MaterialsLayoutCreateInfo = {};
    MaterialsLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    MaterialsLayoutCreateInfo.bindingCount = MaterialsLayoutBindings.Count;
    MaterialsLayoutCreateInfo.pBindings = MaterialsLayoutBindings.Data;
    vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &MaterialsLayoutCreateInfo, NULL,
                                                        &State->DescriptorSetLayouts.Materials),
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
        vtk::descriptor_set* TexturesDS = ctk::push(&State->DescriptorSets.Textures, Assets->Textures.Keys[IterationIndex]);
        TexturesDS->Instances.Count = 1;

        VkDescriptorSetAllocateInfo TexturesAllocateInfo = {};
        TexturesAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        TexturesAllocateInfo.descriptorPool = State->DescriptorPool;
        TexturesAllocateInfo.descriptorSetCount = TexturesDS->Instances.Count;
        TexturesAllocateInfo.pSetLayouts = &State->DescriptorSetLayouts.Textures;
        vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &TexturesAllocateInfo, TexturesDS->Instances.Data),
                                "vkAllocateDescriptorSets", "failed to allocate descriptor sets");
    }

    // Materials
    MaterialsDS->Instances.Count = Swapchain->Images.Count;

    ctk::sarray<VkDescriptorSetLayout, 4> MaterialsDuplicateLayouts = {};
    CTK_ITERATE(Swapchain->Images.Count) ctk::push(&MaterialsDuplicateLayouts, State->DescriptorSetLayouts.Materials);

    VkDescriptorSetAllocateInfo MaterialsAllocateInfo = {};
    MaterialsAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    MaterialsAllocateInfo.descriptorPool = State->DescriptorPool;
    MaterialsAllocateInfo.descriptorSetCount = MaterialsDuplicateLayouts.Count;
    MaterialsAllocateInfo.pSetLayouts = MaterialsDuplicateLayouts.Data;
    vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &MaterialsAllocateInfo, MaterialsDS->Instances.Data),
                            "vkAllocateDescriptorSets", "failed to allocate descriptor sets");

    // Updates
    ctk::sarray<VkDescriptorBufferInfo, 32> DescriptorBufferInfos = {};
    ctk::sarray<VkDescriptorImageInfo, 32> DescriptorImageInfos = {};
    ctk::sarray<VkWriteDescriptorSet, 32> WriteDescriptorSets = {};

    // EntityMatrixes
    CTK_ITERATE(EntityMatrixesDS->Instances.Count) {
        vtk::region* EntityMatrixesRegion = State->UniformBuffers.EntityMatrixes.Regions + IterationIndex;

        VkDescriptorBufferInfo* EntityMatrixDescriptorBufferInfo = ctk::push(&DescriptorBufferInfos);
        EntityMatrixDescriptorBufferInfo->buffer = EntityMatrixesRegion->Buffer->Handle;
        EntityMatrixDescriptorBufferInfo->offset = EntityMatrixesRegion->Offset;
        EntityMatrixDescriptorBufferInfo->range = EntityMatrixesRegion->Size;

        VkWriteDescriptorSet* EntityMatrixesWrite = ctk::push(&WriteDescriptorSets);
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
        vtk::region* LightMatrixesRegion = State->UniformBuffers.LightMatrixes.Regions + IterationIndex;

        VkDescriptorBufferInfo* LightMatrixDescriptorBufferInfo = ctk::push(&DescriptorBufferInfos);
        LightMatrixDescriptorBufferInfo->buffer = LightMatrixesRegion->Buffer->Handle;
        LightMatrixDescriptorBufferInfo->offset = LightMatrixesRegion->Offset;
        LightMatrixDescriptorBufferInfo->range = LightMatrixesRegion->Size;

        VkWriteDescriptorSet* LightMatrixesWrite = ctk::push(&WriteDescriptorSets);
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
        vtk::region* LightsRegion = State->UniformBuffers.Lights.Regions + IterationIndex;

        VkDescriptorBufferInfo* LightsDescriptorBufferInfo = ctk::push(&DescriptorBufferInfos);
        LightsDescriptorBufferInfo->buffer = LightsRegion->Buffer->Handle;
        LightsDescriptorBufferInfo->offset = LightsRegion->Offset;
        LightsDescriptorBufferInfo->range = LightsRegion->Size;

        VkWriteDescriptorSet* LightsWrite = ctk::push(&WriteDescriptorSets);
        LightsWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        LightsWrite->dstSet = LightsDS->Instances[IterationIndex];
        LightsWrite->dstBinding = 0;
        LightsWrite->dstArrayElement = 0;
        LightsWrite->descriptorCount = 1;
        LightsWrite->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        LightsWrite->pBufferInfo = LightsDescriptorBufferInfo;
    }

    // InputAttachments
    VkDescriptorImageInfo* AlbedoInputDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
    AlbedoInputDescriptorImageInfo->sampler = VK_NULL_HANDLE;
    AlbedoInputDescriptorImageInfo->imageView = State->AttachmentImages.Albedo.View;
    AlbedoInputDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet* AlbedoInputImageWrite = ctk::push(&WriteDescriptorSets);
    AlbedoInputImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    AlbedoInputImageWrite->dstSet = InputAttachmentsDS->Instances[0];
    AlbedoInputImageWrite->dstBinding = 0;
    AlbedoInputImageWrite->dstArrayElement = 0;
    AlbedoInputImageWrite->descriptorCount = 1;
    AlbedoInputImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    AlbedoInputImageWrite->pImageInfo = AlbedoInputDescriptorImageInfo;

    VkDescriptorImageInfo* PositionInputDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
    PositionInputDescriptorImageInfo->sampler = VK_NULL_HANDLE;
    PositionInputDescriptorImageInfo->imageView = State->AttachmentImages.Position.View;
    PositionInputDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet* PositionInputImageWrite = ctk::push(&WriteDescriptorSets);
    PositionInputImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    PositionInputImageWrite->dstSet = InputAttachmentsDS->Instances[0];
    PositionInputImageWrite->dstBinding = 1;
    PositionInputImageWrite->dstArrayElement = 0;
    PositionInputImageWrite->descriptorCount = 1;
    PositionInputImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    PositionInputImageWrite->pImageInfo = PositionInputDescriptorImageInfo;

    VkDescriptorImageInfo* NormalInputDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
    NormalInputDescriptorImageInfo->sampler = VK_NULL_HANDLE;
    NormalInputDescriptorImageInfo->imageView = State->AttachmentImages.Normal.View;
    NormalInputDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet* NormalInputImageWrite = ctk::push(&WriteDescriptorSets);
    NormalInputImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    NormalInputImageWrite->dstSet = InputAttachmentsDS->Instances[0];
    NormalInputImageWrite->dstBinding = 2;
    NormalInputImageWrite->dstArrayElement = 0;
    NormalInputImageWrite->descriptorCount = 1;
    NormalInputImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    NormalInputImageWrite->pImageInfo = NormalInputDescriptorImageInfo;

    VkDescriptorImageInfo* MaterialIndexInputDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
    MaterialIndexInputDescriptorImageInfo->sampler = VK_NULL_HANDLE;
    MaterialIndexInputDescriptorImageInfo->imageView = State->AttachmentImages.MaterialIndex.View;
    MaterialIndexInputDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet* MaterialIndexInputImageWrite = ctk::push(&WriteDescriptorSets);
    MaterialIndexInputImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    MaterialIndexInputImageWrite->dstSet = InputAttachmentsDS->Instances[0];
    MaterialIndexInputImageWrite->dstBinding = 3;
    MaterialIndexInputImageWrite->dstArrayElement = 0;
    MaterialIndexInputImageWrite->descriptorCount = 1;
    MaterialIndexInputImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    MaterialIndexInputImageWrite->pImageInfo = MaterialIndexInputDescriptorImageInfo;

    // Textures
    CTK_ITERATE(Assets->Textures.Count) {
        vtk::texture* Texture = Assets->Textures.Values + IterationIndex;

        VkDescriptorImageInfo* TextureDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
        TextureDescriptorImageInfo->sampler = Texture->Sampler;
        TextureDescriptorImageInfo->imageView = Texture->Image.View;
        TextureDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vtk::descriptor_set* TexturesDS = ctk::at(&State->DescriptorSets.Textures, Assets->Textures.Keys[IterationIndex]);
        VkWriteDescriptorSet* TextureImageWrite = ctk::push(&WriteDescriptorSets);
        TextureImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        TextureImageWrite->dstSet = TexturesDS->Instances[0];
        TextureImageWrite->dstBinding = 0;
        TextureImageWrite->dstArrayElement = 0;
        TextureImageWrite->descriptorCount = 1;
        TextureImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        TextureImageWrite->pImageInfo = TextureDescriptorImageInfo;
    }

    // Materials
    CTK_ITERATE(MaterialsDS->Instances.Count) {
        vtk::region* MaterialsRegion = State->UniformBuffers.Materials.Regions + IterationIndex;

        VkDescriptorBufferInfo* MaterialsDescriptorBufferInfo = ctk::push(&DescriptorBufferInfos);
        MaterialsDescriptorBufferInfo->buffer = MaterialsRegion->Buffer->Handle;
        MaterialsDescriptorBufferInfo->offset = MaterialsRegion->Offset;
        MaterialsDescriptorBufferInfo->range = MaterialsRegion->Size;

        VkWriteDescriptorSet* MaterialsWrite = ctk::push(&WriteDescriptorSets);
        MaterialsWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        MaterialsWrite->dstSet = MaterialsDS->Instances[IterationIndex];
        MaterialsWrite->dstBinding = 0;
        MaterialsWrite->dstArrayElement = 0;
        MaterialsWrite->descriptorCount = 1;
        MaterialsWrite->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        MaterialsWrite->pBufferInfo = MaterialsDescriptorBufferInfo;
    }

    vkUpdateDescriptorSets(Device->Logical, WriteDescriptorSets.Count, WriteDescriptorSets.Data, 0, NULL);

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////
    vtk::graphics_pipeline_info DeferredGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&DeferredGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_deferred_vert"));
    ctk::push(&DeferredGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_deferred_frag"));
    ctk::push(&DeferredGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.EntityMatrixes);
    ctk::push(&DeferredGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.Textures);
    ctk::push(&DeferredGPInfo.PushConstantRanges, { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(u32) });
    ctk::push(&DeferredGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    ctk::push(&DeferredGPInfo.VertexInputs, { 0, 1, State->VertexAttributeIndexes.Normal });
    ctk::push(&DeferredGPInfo.VertexInputs, { 0, 2, State->VertexAttributeIndexes.UV });
    DeferredGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&DeferredGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&DeferredGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&DeferredGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    ctk::push(&DeferredGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    ctk::push(&DeferredGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    ctk::push(&DeferredGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    DeferredGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    DeferredGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    DeferredGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    State->GraphicsPipelines.Deferred = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, 0, &DeferredGPInfo);

    vtk::graphics_pipeline_info LightingGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&LightingGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_lighting_vert"));
    ctk::push(&LightingGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_lighting_frag"));
    ctk::push(&LightingGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.InputAttachments);
    ctk::push(&LightingGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.Lights);
    ctk::push(&LightingGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.Materials);
    ctk::push(&LightingGPInfo.PushConstantRanges, { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(lighting_push_constants) });
    ctk::push(&LightingGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    LightingGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&LightingGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&LightingGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&LightingGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    State->GraphicsPipelines.Lighting = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, 1, &LightingGPInfo);

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
    State->GraphicsPipelines.UnlitColor = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPass, 2, &UnlitColorGPInfo);
}

static void set_attenuation_values(light* Light, u32 AttenuationIndex) {
    CTK_ASSERT(AttenuationIndex < ATTENUATION_VALUE_COUNT);
    auto AttenuationValues = ATTENUATION_VALUES + AttenuationIndex;
    Light->Linear = AttenuationValues->X;
    Light->Quadratic = AttenuationValues->Y;
}

static void create_scene(state* State, assets* Assets, vulkan_instance* VulkanInstance) {
    scene* Scene = &State->Scene;
    vtk::swapchain* Swapchain = &VulkanInstance->Swapchain;
    ctk::data SceneData = ctk::load_data("assets/scenes/lighting_test.ctkd");

    // Camera
    ctk::data* CameraData = ctk::at(&SceneData, "camera");
    Scene->Camera.Transform = load_transform(ctk::at(CameraData, "transform"));
    Scene->Camera.FieldOfView = ctk::to_f32(CameraData, "field_of_view");
    Scene->Camera.Aspect = Swapchain->Extent.width / (f32)Swapchain->Extent.height;
    Scene->Camera.ZNear = ctk::to_f32(CameraData, "z_near");
    Scene->Camera.ZFar = ctk::to_f32(CameraData, "z_far");

    // EntityMatrixes
    ctk::data* EntityMap = ctk::at(&SceneData, "entities");
    for(u32 EntityIndex = 0; EntityIndex < EntityMap->Children.Count; ++EntityIndex) {
        ctk::data* EntityData = ctk::at(EntityMap, EntityIndex);
        entity* Entity = push_entity(Scene, EntityData->Key.Data);
        Entity->Transform = load_transform(ctk::at(EntityData, "transform"));
        Entity->TextureDS = ctk::at(&State->DescriptorSets.Textures, ctk::to_cstr(EntityData, "texture"));
        Entity->Mesh = ctk::at(&Assets->Meshes, ctk::to_cstr(EntityData, "mesh"));
        ctk::find(&State->Materials, ctk::to_cstr(EntityData, "material"), &Entity->MaterialIndex);
    }

    // Lights
    ctk::data* LightArray = ctk::at(&SceneData, "lights");
    for(u32 LightIndex = 0; LightIndex < LightArray->Children.Count; ++LightIndex) {
        ctk::data* LightData = ctk::at(LightArray, LightIndex);
        u32 AttenuationIndex = ctk::to_u32(LightData, "attenuation_index");
        transform* LightTransform = NULL;
        light* Light = push_light(Scene, AttenuationIndex, &LightTransform);
        *LightTransform = load_transform(ctk::at(LightData, "transform"));
        Light->Color = load_vec4(ctk::at(LightData, "color"));
        Light->Intensity = ctk::to_f32(LightData, "intensity");
        set_attenuation_values(Light, AttenuationIndex);
    }

    // Cleanup
    ctk::_free(&SceneData);
}

static state* create_state(vulkan_instance* VulkanInstance, assets* Assets) {
    auto State = ctk::allocate<state>();
    *State = {};
    create_test_assets(State);
    create_vulkan_state(State, VulkanInstance, Assets);
    create_scene(State, Assets, VulkanInstance);
    return State;
}

static void record_render_command_buffer(vulkan_instance* VulkanInstance, assets* Assets, state* State, u32 SwapchainImageIndex) {
    vtk::device* Device = &VulkanInstance->Device;
    vtk::swapchain* Swapchain = &VulkanInstance->Swapchain;

    scene* Scene = &State->Scene;
    vtk::render_pass* RenderPass = &State->RenderPass;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = 0;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

    VkCommandBuffer CommandBuffer = RenderPass->CommandBuffers[SwapchainImageIndex];
    vtk::validate_vk_result(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                            "vkBeginCommandBuffer", "failed to begin recording command buffer");

    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = Swapchain->Extent;

    VkRenderPassBeginInfo RenderPassBeginInfo = {};
    RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RenderPassBeginInfo.renderPass = RenderPass->Handle;
    RenderPassBeginInfo.framebuffer = RenderPass->Framebuffers[SwapchainImageIndex];
    RenderPassBeginInfo.renderArea = RenderArea;
    RenderPassBeginInfo.clearValueCount = RenderPass->ClearValues.Count;
    RenderPassBeginInfo.pClearValues = RenderPass->ClearValues.Data;

    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        // Deferred Stage
        {
            vtk::graphics_pipeline* DeferredGP = &State->GraphicsPipelines.Deferred;
            vtk::descriptor_set* DescriptorSets[] = { &State->DescriptorSets.EntityMatrixes };
            for(u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex) {
                entity* Entity = Scene->Entities.Values + EntityIndex;
                VkDescriptorSet* TextureDS = Entity->TextureDS->Instances + 0;
                mesh* Mesh = Entity->Mesh;

                vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DeferredGP->Handle);
                vtk::bind_descriptor_sets(CommandBuffer, DeferredGP->Layout, 0, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets),
                                          SwapchainImageIndex, EntityIndex);
                vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DeferredGP->Layout,
                                        1, 1, TextureDS,
                                        0, NULL);
                vkCmdPushConstants(CommandBuffer, DeferredGP->Layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(u32), &Entity->MaterialIndex);
                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
                vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
            }
        }

        vkCmdNextSubpass(CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

        // Lighting Stage
        {
            vtk::graphics_pipeline* LightingGP = &State->GraphicsPipelines.Lighting;
            mesh* FullscreenPlane = ctk::at(&Assets->Meshes, "fullscreen_plane");
            VkDescriptorSet DescriptorSets[] = {
                State->DescriptorSets.InputAttachments.Instances[0],
                State->DescriptorSets.Lights.Instances[SwapchainImageIndex],
                State->DescriptorSets.Materials.Instances[SwapchainImageIndex],
            };
            State->PushConstants.Lighting = {
                Scene->Camera.Transform.Position,
                State->LightMode,
            };
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingGP->Handle);
            vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingGP->Layout,
                                    0, CTK_ARRAY_COUNT(DescriptorSets), DescriptorSets,
                                    0, NULL);
            vkCmdPushConstants(CommandBuffer, LightingGP->Layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(lighting_push_constants), &State->PushConstants.Lighting);
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &FullscreenPlane->VertexRegion.Buffer->Handle,
                                   &FullscreenPlane->VertexRegion.Offset);
            vkCmdBindIndexBuffer(CommandBuffer, FullscreenPlane->IndexRegion.Buffer->Handle, FullscreenPlane->IndexRegion.Offset,
                                 VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(CommandBuffer, FullscreenPlane->Indexes.Count, 1, 0, 0, 0);
        }

        vkCmdNextSubpass(CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

        // Direct Stage
        {
            // Draw Light Diamonds
            vtk::graphics_pipeline* UnlitColorGP = &State->GraphicsPipelines.UnlitColor;
            mesh* LightDiamond = ctk::at(&Assets->Meshes, "light_diamond");
            vtk::descriptor_set* DescriptorSets[] = { &State->DescriptorSets.LightMatrixes };
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UnlitColorGP->Handle);

            // Point Lights
            for(u32 LightIndex = 0; LightIndex < Scene->Lights.Count; ++LightIndex) {
                vtk::bind_descriptor_sets(CommandBuffer, UnlitColorGP->Layout, 0, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets),
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
    vkCmdEndRenderPass(CommandBuffer);
    vtk::validate_vk_result(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
}

static void update_entity_matrixes(VkDevice LogicalDevice, scene* Scene, glm::mat4 ViewProjectionMatrix, vtk::region* Region) {
    if(Scene->Entities.Count == 0) return;

    // Entity Model Matrixes
    for(u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex) {
        transform* EntityTransform = &Scene->Entities.Values[EntityIndex].Transform;
        matrix_ubo* EntityMatrixUBO = Scene->EntityMatrixUBOs + EntityIndex;
        glm::mat4 ModelMatrix(1.0f);
        ModelMatrix = glm::translate(ModelMatrix, { EntityTransform->Position.X, EntityTransform->Position.Y, EntityTransform->Position.Z });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(EntityTransform->Rotation.X), { 1.0f, 0.0f, 0.0f });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(EntityTransform->Rotation.Y), { 0.0f, 1.0f, 0.0f });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(EntityTransform->Rotation.Z), { 0.0f, 0.0f, 1.0f });
        ModelMatrix = glm::scale(ModelMatrix, { EntityTransform->Scale.X, EntityTransform->Scale.Y, EntityTransform->Scale.Z });
        EntityMatrixUBO->ModelMatrix = ModelMatrix;
        EntityMatrixUBO->ModelViewProjectionMatrix = ViewProjectionMatrix * ModelMatrix;
    }

    vtk::write_to_host_region(LogicalDevice, Region, Scene->EntityMatrixUBOs.Data, ctk::byte_count(&Scene->EntityMatrixUBOs), 0);
}

static void update_lights(VkDevice LogicalDevice, state* State, glm::mat4 ViewProjectionMatrix, u32 SwapchainImageIndex) {
    scene *Scene = &State->Scene;
    if(Scene->Lights.Count == 0) return;

    for(u32 LightIndex = 0; LightIndex < Scene->Lights.Count; ++LightIndex) {
        ctk::vec3<f32>* LightPosition = &Scene->Lights[LightIndex].Position;
        matrix_ubo* LightMatrixUBO = Scene->LightMatrixUBOs + LightIndex;
        transform* LightTransform = Scene->LightTransforms + LightIndex;

        // Position
        *LightPosition = LightTransform->Position;

        // Matrix
        glm::mat4 ModelMatrix(1.0f);
        ModelMatrix = glm::translate(ModelMatrix, { LightPosition->X, LightPosition->Y, LightPosition->Z });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(LightTransform->Rotation.X), { 1.0f, 0.0f, 0.0f });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(LightTransform->Rotation.Y), { 0.0f, 1.0f, 0.0f });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(LightTransform->Rotation.Z), { 0.0f, 0.0f, 1.0f });
        ModelMatrix = glm::scale(ModelMatrix, { 0.25f, 0.25f, 0.25f });
        LightMatrixUBO->ModelMatrix = ModelMatrix;
        LightMatrixUBO->ModelViewProjectionMatrix = ViewProjectionMatrix * ModelMatrix;
    }

    vtk::write_to_host_region(LogicalDevice, State->UniformBuffers.LightMatrixes.Regions + SwapchainImageIndex,
                              Scene->LightMatrixUBOs.Data, ctk::byte_count(&Scene->LightMatrixUBOs), 0);
    vtk::write_to_host_region(LogicalDevice, State->UniformBuffers.Lights.Regions + SwapchainImageIndex,
                              &Scene->Lights, sizeof(Scene->Lights), 0);
}

static void update_scene(VkDevice LogicalDevice, input_state* InputState, state* State, u32 SwapchainImageIndex) {
    scene* Scene = &State->Scene;
    glm::mat4 ViewProjectionMatrix = view_projection_matrix(&Scene->Camera);
    update_entity_matrixes(LogicalDevice, Scene, ViewProjectionMatrix, State->UniformBuffers.EntityMatrixes.Regions + SwapchainImageIndex);
    update_lights(LogicalDevice, State, ViewProjectionMatrix, SwapchainImageIndex);
    vtk::write_to_host_region(LogicalDevice, State->UniformBuffers.Materials.Regions + SwapchainImageIndex,
                              State->Materials.Values, ctk::values_byte_count(&State->Materials), 0);
}

static void draw_ui(state *State) {
    scene* Scene = &State->Scene;
    control_state* ControlState = &State->ControlState;
    CTK_ITERATE(Scene->Lights.Count) {
        u32* AttenuationIndex = Scene->LightAttenuationIndexes + IterationIndex;
        char Buffer[256] = {};
        sprintf(Buffer, "light %u attenuation index", IterationIndex);
        ImGui::SliderInt(Buffer, (s32*)AttenuationIndex, 0, ATTENUATION_VALUE_COUNT - 1);
        set_attenuation_values(Scene->Lights + IterationIndex, *AttenuationIndex);
    }

    CTK_ITERATE(State->Materials.Count) {
        material *Material = State->Materials.Values + IterationIndex;
        char Buffer[256] = {};
        sprintf(Buffer, "material \"%s\" shine exponent", State->Materials.Keys + IterationIndex);
        ImGui::SliderInt(Buffer, (s32*)&Material->ShineExponent, 0, 256);
    }

    cstr ControlModes[] = { "entity", "light" };
    ImGui::ListBox("control mode", (s32*)&ControlState->Mode, ControlModes, CTK_ARRAY_COUNT(ControlModes));

    cstr EntityModes[] = { "translate", "rotate", "scale" };
    ImGui::ListBox("transform mode", (s32*)&ControlState->TransformMode, EntityModes, CTK_ARRAY_COUNT(EntityModes));

    if(ControlState->Mode == control_state::MODE_ENTITY) {
        ctk::sarray<char*, 1024> EntityList = {};
        CTK_ITERATE(Scene->Entities.Count) ctk::push(&EntityList, (char*)&Scene->Entities.Keys[IterationIndex]);
        ImGui::ListBox("entities", (s32*)&ControlState->EntityIndex, EntityList.Data, EntityList.Count);
    } else {
        ctk::sarray<char*, 64> LightList = {};
        CTK_ITERATE(Scene->Lights.Count) {
            char* Name = ctk::allocate<char>(64);
            sprintf(Name, "light %u", IterationIndex);
            ctk::push(&LightList, Name);
        }
        ImGui::ListBox("lights", (s32*)&ControlState->LightIndex, LightList.Data, LightList.Count);
        CTK_ITERATE(LightList.Count) ::free(LightList[IterationIndex]);
    }
}

static void controls(state* State, input_state* InputState) {
    scene* Scene = &State->Scene;
    control_state* ControlState = &State->ControlState;

    // View Mode
         if(InputState->KeyDown[GLFW_KEY_F1]) State->LightMode = state::LIGHT_MODE_COMPOSITE;
    else if(InputState->KeyDown[GLFW_KEY_F2]) State->LightMode = state::LIGHT_MODE_ALBEDO;
    else if(InputState->KeyDown[GLFW_KEY_F3]) State->LightMode = state::LIGHT_MODE_POSITION;
    else if(InputState->KeyDown[GLFW_KEY_F4]) State->LightMode = state::LIGHT_MODE_NORMAL;

    // Camera
    camera_controls(&Scene->Camera.Transform, InputState);

    // Entities/Lights
    transform* Transform = ControlState->Mode == control_state::MODE_ENTITY
                           ? &Scene->Entities.Values[ControlState->EntityIndex].Transform
                           : Scene->LightTransforms + ControlState->LightIndex;
    ctk::vec3<f32>* TransformProperty = ControlState->TransformMode == control_state::TRANSFORM_TRANSLATE ? &Transform->Position :
                                        ControlState->TransformMode == control_state::TRANSFORM_ROTATE ? &Transform->Rotation :
                                        &Transform->Scale;
    f32 Modifier = InputState->KeyDown[GLFW_KEY_LEFT_SHIFT] ? 4 :
                   InputState->KeyDown[GLFW_KEY_LEFT_CONTROL] ? 1 :
                   2;
    f32 Step = ControlState->TransformMode == control_state::TRANSFORM_TRANSLATE ? 0.01 :
               ControlState->TransformMode == control_state::TRANSFORM_ROTATE ? 0.1f :
               0.01f;
    if(InputState->KeyDown[GLFW_KEY_L]) TransformProperty->X += Step * Modifier;
    if(InputState->KeyDown[GLFW_KEY_J]) TransformProperty->X -= Step * Modifier;
    if(InputState->KeyDown[GLFW_KEY_O]) TransformProperty->Y -= Step * Modifier;
    if(InputState->KeyDown[GLFW_KEY_U]) TransformProperty->Y += Step * Modifier;
    if(InputState->KeyDown[GLFW_KEY_I]) TransformProperty->Z += Step * Modifier;
    if(InputState->KeyDown[GLFW_KEY_K]) TransformProperty->Z -= Step * Modifier;
}

static void test_main() {
    input_state InputState = {};
    window* Window = create_window(&InputState);
    vulkan_instance* VulkanInstance = create_vulkan_instance(Window);
    assets* Assets = create_assets(VulkanInstance);
    state* State = create_state(VulkanInstance, Assets);
    ui* UI = create_ui(Window, VulkanInstance);
    while(!glfwWindowShouldClose(Window->Handle)) {
        // Check if window should close.
        glfwPollEvents();
        if(InputState.KeyDown[GLFW_KEY_ESCAPE]) break;

        // Process input.
        update_input_state(&InputState, Window->Handle);
        controls(State, &InputState);

        // Process frame.
        u32 SwapchainImageIndex = aquire_next_swapchain_image_index(VulkanInstance);
        record_render_command_buffer(VulkanInstance, Assets, State, SwapchainImageIndex);
        ui_new_frame();
        draw_ui(State);
        record_ui_command_buffer(VulkanInstance, UI, SwapchainImageIndex);
        update_scene(VulkanInstance->Device.Logical, &InputState, State, SwapchainImageIndex);
        synchronize_current_frame(VulkanInstance, SwapchainImageIndex);
        vtk::render_pass* RenderPasses[] = { &State->RenderPass, &UI->RenderPass };
        submit_render_passes(VulkanInstance, RenderPasses, CTK_ARRAY_COUNT(RenderPasses), SwapchainImageIndex);
        cycle_frame(VulkanInstance);
        Sleep(1);
    }
}
