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
    glm::mat4 ModelMatrix;
    glm::mat4 ModelViewProjectionMatrix;
};

struct light {
    ctk::vec4<f32> Color;
    ctk::vec3<f32> Position;
    f32 Linear;
    f32 Quadratic;
    f32 Intensity;
    f32 AmbientIntensity;
    alignas(16) glm::mat4 view_proj_mtx;
};

struct entity {
    transform Transform;
    vtk::descriptor_set *TextureDS;
    mesh *Mesh;
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
        MODE_MATERIAL,
    };
    enum {
        TRANSFORM_TRANSLATE,
        TRANSFORM_ROTATE,
        TRANSFORM_SCALE,
    };
    s32 Mode;
    s32 TransformMode;
    u32 EntityIndex;
    u32 LightIndex;
    u32 MaterialIndex;
};

struct state {
    static const u32 MAX_MATERIALS = 16;
    static const u32 SHADOW_MAP_SIZE = 10000;
    scene Scene;
    control_state ControlState;
    struct {
        vtk::render_pass Main;
        vtk::render_pass ShadowMap;
    } RenderPasses;
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
        vtk::uniform_buffer ShadowMapEntityMatrixes;
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
    vtk::texture ShadowMaps[1];
    struct {
        VkDescriptorSetLayout entity_matrixes;
        VkDescriptorSetLayout Lights;
        VkDescriptorSetLayout InputAttachments;
        VkDescriptorSetLayout Textures;
        VkDescriptorSetLayout Materials;
        VkDescriptorSetLayout ShadowMaps;
    } DescriptorSetLayouts;
    struct {
        vtk::descriptor_set EntityMatrixes;
        vtk::descriptor_set ShadowMapEntityMatrixes;
        vtk::descriptor_set LightMatrixes;
        vtk::descriptor_set Lights;
        vtk::descriptor_set InputAttachments;
        ctk::smap<vtk::descriptor_set, 16> Textures;
        vtk::descriptor_set Materials;
        vtk::descriptor_set ShadowMaps;
    } DescriptorSets;
    struct {
        vtk::graphics_pipeline Deferred;
        vtk::graphics_pipeline Lighting;
        vtk::graphics_pipeline UnlitColor;
        vtk::graphics_pipeline ShadowMap;
    } GraphicsPipelines;
    struct {
        lighting_push_constants Lighting;
    } PushConstants;
    struct {
        VkSemaphore ShadowMapFinished;
    } Semaphores;
    struct {
        VkFence ShadowMapFinished;
    } Fences;
    ctk::smap<material, MAX_MATERIALS> Materials;
};

struct App {
    input_state *InputState;
    window *Window;
    vulkan_instance *VulkanInstance;
    assets *Assets;
    state *State;
    ui *UI;
};

static void key_callback(GLFWwindow* Window, s32 Key, s32 Scancode, s32 Action, s32 Mods) {
    auto app = (App *)glfwGetWindowUserPointer(Window);
    app->InputState->KeyDown[Key] = Action == GLFW_PRESS || Action == GLFW_REPEAT;
}

static void mouse_button_callback(GLFWwindow* Window, s32 Button, s32 Action, s32 Mods) {
    auto app = (App *)glfwGetWindowUserPointer(Window);
    app->InputState->MouseButtonDown[Button] = Action == GLFW_PRESS || Action == GLFW_REPEAT;
}

static entity *push_entity(scene *Scene, cstr Name) {
    ctk::push(&Scene->EntityMatrixUBOs);
    return ctk::push(&Scene->Entities, Name);
}

static light *push_light(scene *Scene, u32 AttenuationIndex, transform** LightTransform) {
    ctk::push(&Scene->LightMatrixUBOs);
    ctk::push(&Scene->LightAttenuationIndexes, AttenuationIndex);
    *LightTransform = ctk::push(&Scene->LightTransforms, default_transform());
    return ctk::push(&Scene->Lights);
}

static void create_test_assets(state *State) {
    // {
    //     material *Material = ctk::push(&State->Materials, "test0");
    //     Material->ShineExponent = 4;
    // }
    // {
    //     material *Material = ctk::push(&State->Materials, "test1");
    //     Material->ShineExponent = 256;
    // }
    // {
    //     material *Material = ctk::push(&State->Materials, "test2");
    //     Material->ShineExponent = 1024;
    // }
    {
        material *Material = ctk::push(&State->Materials, "blinn_phong");
        Material->ShineExponent = 64;
    }
}

static void create_descriptor_sets(state *State, vulkan_instance *VulkanInstance, assets *Assets) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    vtk::descriptor_set *EntityMatrixesDS = &State->DescriptorSets.EntityMatrixes;
    vtk::descriptor_set *ShadowMapEntityMatrixesDS = &State->DescriptorSets.ShadowMapEntityMatrixes;
    vtk::descriptor_set *LightMatrixesDS = &State->DescriptorSets.LightMatrixes;
    vtk::descriptor_set *LightsDS = &State->DescriptorSets.Lights;
    vtk::descriptor_set *InputAttachmentsDS = &State->DescriptorSets.InputAttachments;
    vtk::descriptor_set *MaterialsDS = &State->DescriptorSets.Materials;
    vtk::descriptor_set *shadow_maps_ds = &State->DescriptorSets.ShadowMaps;

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

    ////////////////////////////////////////////////////////////
    /// Layouts
    ////////////////////////////////////////////////////////////

    /* entity_matrixes */ {
        VkDescriptorSetLayoutBinding binding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;
        vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &info, NULL, &State->DescriptorSetLayouts.entity_matrixes),
                                "vkCreateDescriptorSetLayout", "error creating descriptor set layout");
    }

    /* Lights */ {
        VkDescriptorSetLayoutBinding binding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT };
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;
        vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &info, NULL, &State->DescriptorSetLayouts.Lights),
                                "vkCreateDescriptorSetLayout", "error creating descriptor set layout");
    }

    /* InputAttachments */ {
        VkDescriptorSetLayoutBinding bindings[] = {
            { 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
            { 1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
            { 2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
            { 3, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        };
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = CTK_ARRAY_COUNT(bindings);
        info.pBindings = bindings;
        vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &info, NULL, &State->DescriptorSetLayouts.InputAttachments),
                                "vkCreateDescriptorSetLayout", "error creating descriptor set layout");
    }

    /* Textures */ {
        VkDescriptorSetLayoutBinding binding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT };
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;
        vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &info, NULL, &State->DescriptorSetLayouts.Textures),
                                "vkCreateDescriptorSetLayout", "error creating descriptor set layout");
    }

    /* Materials */ {
        VkDescriptorSetLayoutBinding binding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT };
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;
        vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &info, NULL, &State->DescriptorSetLayouts.Materials),
                                "vkCreateDescriptorSetLayout", "error creating descriptor set layout");
    }

    /* Shadow Maps */ {
        VkDescriptorSetLayoutBinding binding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT };
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;
        vtk::validate_vk_result(vkCreateDescriptorSetLayout(Device->Logical, &info, NULL, &State->DescriptorSetLayouts.ShadowMaps),
                                "vkCreateDescriptorSetLayout", "error creating descriptor set layout");
    }

    ////////////////////////////////////////////////////////////
    /// Allocation
    ////////////////////////////////////////////////////////////

    /* EntityMatrixes */ {
        ctk::sarray<VkDescriptorSetLayout, 4> duplicate_layouts = {};
        CTK_ITERATE(Swapchain->Images.Count)
            ctk::push(&duplicate_layouts, State->DescriptorSetLayouts.entity_matrixes);

        EntityMatrixesDS->Instances.Count = Swapchain->Images.Count;
        ctk::push(&EntityMatrixesDS->DynamicOffsets, State->UniformBuffers.EntityMatrixes.ElementSize);

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = State->DescriptorPool;
        alloc_info.descriptorSetCount = duplicate_layouts.Count;
        alloc_info.pSetLayouts = duplicate_layouts.Data;
        vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &alloc_info, EntityMatrixesDS->Instances.Data),
                                "vkAllocateDescriptorSets", "failed to allocate descriptor sets");
    }

    /* ShadowMapEntityMatrixes */ {
        ctk::sarray<VkDescriptorSetLayout, 4> duplicate_layouts = {};
        CTK_ITERATE(Swapchain->Images.Count)
            ctk::push(&duplicate_layouts,State->DescriptorSetLayouts.entity_matrixes);

        ShadowMapEntityMatrixesDS->Instances.Count = Swapchain->Images.Count;
        ctk::push(&ShadowMapEntityMatrixesDS->DynamicOffsets, State->UniformBuffers.ShadowMapEntityMatrixes.ElementSize);

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = State->DescriptorPool;
        alloc_info.descriptorSetCount = duplicate_layouts.Count;
        alloc_info.pSetLayouts = duplicate_layouts.Data;
        vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &alloc_info, ShadowMapEntityMatrixesDS->Instances.Data),
                                "vkAllocateDescriptorSets", "failed to allocate descriptor sets");
    }

    /* LightMatrixes */ {
        ctk::sarray<VkDescriptorSetLayout, 4> duplicate_layouts = {};
        CTK_ITERATE(Swapchain->Images.Count)
            ctk::push(&duplicate_layouts, State->DescriptorSetLayouts.entity_matrixes);

        LightMatrixesDS->Instances.Count = Swapchain->Images.Count;
        ctk::push(&LightMatrixesDS->DynamicOffsets, State->UniformBuffers.LightMatrixes.ElementSize);

        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = State->DescriptorPool;
        alloc_info.descriptorSetCount = duplicate_layouts.Count;
        alloc_info.pSetLayouts = duplicate_layouts.Data;
        vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &alloc_info, LightMatrixesDS->Instances.Data),
                                "vkAllocateDescriptorSets", "failed to allocate descriptor sets");
    }

    // Lights
    LightsDS->Instances.Count = Swapchain->Images.Count;

    ctk::sarray<VkDescriptorSetLayout, 4> LightsDuplicateLayouts = {};
    CTK_ITERATE(Swapchain->Images.Count)
        ctk::push(&LightsDuplicateLayouts, State->DescriptorSetLayouts.Lights);

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

    // Materials
    MaterialsDS->Instances.Count = Swapchain->Images.Count;

    ctk::sarray<VkDescriptorSetLayout, 4> MaterialsDuplicateLayouts = {};
    CTK_ITERATE(Swapchain->Images.Count)
        ctk::push(&MaterialsDuplicateLayouts, State->DescriptorSetLayouts.Materials);

    VkDescriptorSetAllocateInfo MaterialsAllocateInfo = {};
    MaterialsAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    MaterialsAllocateInfo.descriptorPool = State->DescriptorPool;
    MaterialsAllocateInfo.descriptorSetCount = MaterialsDuplicateLayouts.Count;
    MaterialsAllocateInfo.pSetLayouts = MaterialsDuplicateLayouts.Data;
    vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &MaterialsAllocateInfo, MaterialsDS->Instances.Data),
                            "vkAllocateDescriptorSets", "failed to allocate descriptor sets");

    // Shadow Maps
    shadow_maps_ds->Instances.Count = 1;
    VkDescriptorSetAllocateInfo shadow_maps_ds_ci = {};
    shadow_maps_ds_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    shadow_maps_ds_ci.descriptorPool = State->DescriptorPool;
    shadow_maps_ds_ci.descriptorSetCount = 1;
    shadow_maps_ds_ci.pSetLayouts = &State->DescriptorSetLayouts.ShadowMaps;
    vtk::validate_vk_result(vkAllocateDescriptorSets(Device->Logical, &shadow_maps_ds_ci, shadow_maps_ds->Instances.Data),
                            "vkAllocateDescriptorSets", "failed to allocate descriptor sets");

    ////////////////////////////////////////////////////////////
    /// Updates
    ////////////////////////////////////////////////////////////
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

    // ShadowMapEntityMatrixes
    CTK_ITERATE(ShadowMapEntityMatrixesDS->Instances.Count) {
        vtk::region *ShadowMapEntityMatrixesRegion = State->UniformBuffers.ShadowMapEntityMatrixes.Regions + IterationIndex;

        VkDescriptorBufferInfo *EntityMatrixDescriptorBufferInfo = ctk::push(&DescriptorBufferInfos);
        EntityMatrixDescriptorBufferInfo->buffer = ShadowMapEntityMatrixesRegion->Buffer->Handle;
        EntityMatrixDescriptorBufferInfo->offset = ShadowMapEntityMatrixesRegion->Offset;
        EntityMatrixDescriptorBufferInfo->range = ShadowMapEntityMatrixesRegion->Size;

        VkWriteDescriptorSet *ShadowMapEntityMatrixesWrite = ctk::push(&WriteDescriptorSets);
        ShadowMapEntityMatrixesWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ShadowMapEntityMatrixesWrite->dstSet = ShadowMapEntityMatrixesDS->Instances[IterationIndex];
        ShadowMapEntityMatrixesWrite->dstBinding = 0;
        ShadowMapEntityMatrixesWrite->dstArrayElement = 0;
        ShadowMapEntityMatrixesWrite->descriptorCount = 1;
        ShadowMapEntityMatrixesWrite->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        ShadowMapEntityMatrixesWrite->pBufferInfo = EntityMatrixDescriptorBufferInfo;
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

    VkDescriptorImageInfo *MaterialIndexInputDescriptorImageInfo = ctk::push(&DescriptorImageInfos);
    MaterialIndexInputDescriptorImageInfo->sampler = VK_NULL_HANDLE;
    MaterialIndexInputDescriptorImageInfo->imageView = State->AttachmentImages.MaterialIndex.View;
    MaterialIndexInputDescriptorImageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet *MaterialIndexInputImageWrite = ctk::push(&WriteDescriptorSets);
    MaterialIndexInputImageWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    MaterialIndexInputImageWrite->dstSet = InputAttachmentsDS->Instances[0];
    MaterialIndexInputImageWrite->dstBinding = 3;
    MaterialIndexInputImageWrite->dstArrayElement = 0;
    MaterialIndexInputImageWrite->descriptorCount = 1;
    MaterialIndexInputImageWrite->descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    MaterialIndexInputImageWrite->pImageInfo = MaterialIndexInputDescriptorImageInfo;

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

    // Materials
    CTK_ITERATE(MaterialsDS->Instances.Count) {
        vtk::region *MaterialsRegion = State->UniformBuffers.Materials.Regions + IterationIndex;

        VkDescriptorBufferInfo *MaterialsDescriptorBufferInfo = ctk::push(&DescriptorBufferInfos);
        MaterialsDescriptorBufferInfo->buffer = MaterialsRegion->Buffer->Handle;
        MaterialsDescriptorBufferInfo->offset = MaterialsRegion->Offset;
        MaterialsDescriptorBufferInfo->range = MaterialsRegion->Size;

        VkWriteDescriptorSet *MaterialsWrite = ctk::push(&WriteDescriptorSets);
        MaterialsWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        MaterialsWrite->dstSet = MaterialsDS->Instances[IterationIndex];
        MaterialsWrite->dstBinding = 0;
        MaterialsWrite->dstArrayElement = 0;
        MaterialsWrite->descriptorCount = 1;
        MaterialsWrite->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        MaterialsWrite->pBufferInfo = MaterialsDescriptorBufferInfo;
    }

    // Shadow Maps
    VkDescriptorImageInfo *shadow_map_img_info = ctk::push(&DescriptorImageInfos);
    shadow_map_img_info->sampler = State->ShadowMaps[0].Sampler;
    shadow_map_img_info->imageView = State->ShadowMaps[0].Image.View;
    shadow_map_img_info->imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet *shadow_maps_write = ctk::push(&WriteDescriptorSets);
    shadow_maps_write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    shadow_maps_write->dstSet = shadow_maps_ds->Instances[0];
    shadow_maps_write->dstBinding = 0;
    shadow_maps_write->dstArrayElement = 0;
    shadow_maps_write->descriptorCount = 1;
    shadow_maps_write->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadow_maps_write->pImageInfo = shadow_map_img_info;

    vkUpdateDescriptorSets(Device->Logical, WriteDescriptorSets.Count, WriteDescriptorSets.Data, 0, NULL);
}

static void create_main_render_pass(state *State, vulkan_instance *VulkanInstance, assets *Assets) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
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
    AlbedoAttachment->ClearValue = { 0, 0, 0, 1 };

    vtk::attachment *PositionAttachment = ctk::push(&RenderPassInfo.Attachments);
    PositionAttachment->Description.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    PositionAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    PositionAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    PositionAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    PositionAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    PositionAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    PositionAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    PositionAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    PositionAttachment->ClearValue = { 0, 0, 0, 1 };

    vtk::attachment *NormalAttachment = ctk::push(&RenderPassInfo.Attachments);
    NormalAttachment->Description.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    NormalAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    NormalAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    NormalAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    NormalAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    NormalAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    NormalAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    NormalAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    NormalAttachment->ClearValue = { 0, 0, 0, 1 };

    vtk::attachment *DepthAttachment = ctk::push(&RenderPassInfo.Attachments);
    DepthAttachment->Description.format = State->AttachmentImages.Depth.Format;
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
    SwapchainAttachment->ClearValue = { 0, 0, 0, 1 };

    vtk::attachment *MaterialIndexAttachment = ctk::push(&RenderPassInfo.Attachments);
    MaterialIndexAttachment->Description.format = VK_FORMAT_R32_UINT;
    MaterialIndexAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    MaterialIndexAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    MaterialIndexAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    MaterialIndexAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    MaterialIndexAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    MaterialIndexAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    MaterialIndexAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    MaterialIndexAttachment->ClearValue = { 0 };

    // Subpasses
    vtk::subpass *DeferredSubpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::push(&DeferredSubpass->ColorAttachmentReferences, { 5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    ctk::set(&DeferredSubpass->DepthAttachmentReference, { 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });

    vtk::subpass *LightingSubpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->InputAttachmentReferences, { 5, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    ctk::push(&LightingSubpass->ColorAttachmentReferences, { 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    vtk::subpass *DirectSubpass = ctk::push(&RenderPassInfo.Subpasses);
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
        vtk::framebuffer_info *FramebufferInfo = ctk::push(&RenderPassInfo.FramebufferInfos);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Albedo.View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Position.View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Normal.View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.Depth.View);
        ctk::push(&FramebufferInfo->Attachments, Swapchain->Images[IterationIndex].View);
        ctk::push(&FramebufferInfo->Attachments, State->AttachmentImages.MaterialIndex.View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    State->RenderPasses.Main = vtk::create_render_pass(Device->Logical, VulkanInstance->GraphicsCommandPool, &RenderPassInfo);

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////
    vtk::graphics_pipeline_info DeferredGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&DeferredGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_deferred_vert"));
    ctk::push(&DeferredGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_deferred_frag"));
    ctk::push(&DeferredGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.entity_matrixes);
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
    State->GraphicsPipelines.Deferred = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPasses.Main, 0, &DeferredGPInfo);

    vtk::graphics_pipeline_info LightingGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&LightingGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_lighting_vert"));
    ctk::push(&LightingGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "lighting_lighting_frag"));
    ctk::push(&LightingGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.InputAttachments);
    ctk::push(&LightingGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.ShadowMaps);
    ctk::push(&LightingGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.Lights);
    ctk::push(&LightingGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.Materials);
    ctk::push(&LightingGPInfo.PushConstantRanges, { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(lighting_push_constants) });
    ctk::push(&LightingGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    ctk::push(&LightingGPInfo.VertexInputs, { 0, 1, State->VertexAttributeIndexes.UV });
    LightingGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&LightingGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&LightingGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&LightingGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    State->GraphicsPipelines.Lighting = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPasses.Main, 1, &LightingGPInfo);

    vtk::graphics_pipeline_info UnlitColorGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&UnlitColorGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "unlit_color_vert"));
    ctk::push(&UnlitColorGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "unlit_color_frag"));
    ctk::push(&UnlitColorGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.entity_matrixes);
    ctk::push(&UnlitColorGPInfo.PushConstantRanges, { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ctk::vec4<f32>) });
    ctk::push(&UnlitColorGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    UnlitColorGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&UnlitColorGPInfo.Viewports, { 0, 0, (f32)Swapchain->Extent.width, (f32)Swapchain->Extent.height, 0, 1 });
    ctk::push(&UnlitColorGPInfo.Scissors, { 0, 0, Swapchain->Extent.width, Swapchain->Extent.height });
    ctk::push(&UnlitColorGPInfo.ColorBlendAttachmentStates, vtk::default_color_blend_attachment_state());
    UnlitColorGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    UnlitColorGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    UnlitColorGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    State->GraphicsPipelines.UnlitColor = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPasses.Main, 2, &UnlitColorGPInfo);
}

static void create_shadow_map_render_pass(state *State, vulkan_instance *VulkanInstance, assets *Assets) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::render_pass_info RenderPassInfo = {};

    // Attachments
    vtk::attachment *DepthAttachment = ctk::push(&RenderPassInfo.Attachments);
    DepthAttachment->Description.format = State->AttachmentImages.Depth.Format;
    DepthAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    DepthAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    DepthAttachment->ClearValue = { 1.0f, 0 };

    // Subpasses
    vtk::subpass *Subpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::set(&Subpass->DepthAttachmentReference, { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });

    // Subpass Dependencies
    RenderPassInfo.SubpassDependencies.Count = 2;
    RenderPassInfo.SubpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    RenderPassInfo.SubpassDependencies[0].dstSubpass = 0;
    RenderPassInfo.SubpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    RenderPassInfo.SubpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    RenderPassInfo.SubpassDependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    RenderPassInfo.SubpassDependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    RenderPassInfo.SubpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    RenderPassInfo.SubpassDependencies[1].srcSubpass = 0;
    RenderPassInfo.SubpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    RenderPassInfo.SubpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    RenderPassInfo.SubpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    RenderPassInfo.SubpassDependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    RenderPassInfo.SubpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    RenderPassInfo.SubpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Framebuffer Infos
    vtk::framebuffer_info *FramebufferInfo = ctk::push(&RenderPassInfo.FramebufferInfos);
    ctk::push(&FramebufferInfo->Attachments, State->ShadowMaps[0].Image.View);
    FramebufferInfo->Extent = { state::SHADOW_MAP_SIZE, state::SHADOW_MAP_SIZE };
    FramebufferInfo->Layers = 1;

    State->RenderPasses.ShadowMap = vtk::create_render_pass(Device->Logical, VulkanInstance->GraphicsCommandPool, &RenderPassInfo);

    // Graphics Pipeline
    vtk::graphics_pipeline_info ShadowMapGPInfo = vtk::default_graphics_pipeline_info();
    ctk::push(&ShadowMapGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "shadow_map_vert"));
    ctk::push(&ShadowMapGPInfo.ShaderModules, ctk::at(&Assets->ShaderModules, "shadow_map_frag"));
    ctk::push(&ShadowMapGPInfo.DescriptorSetLayouts, State->DescriptorSetLayouts.entity_matrixes);
    ctk::push(&ShadowMapGPInfo.VertexInputs, { 0, 0, State->VertexAttributeIndexes.Position });
    ShadowMapGPInfo.VertexLayout = &State->VertexLayout;
    ctk::push(&ShadowMapGPInfo.Viewports, { 0, 0, (f32)state::SHADOW_MAP_SIZE, (f32)state::SHADOW_MAP_SIZE, 0, 1 });
    ctk::push(&ShadowMapGPInfo.Scissors, { 0, 0, state::SHADOW_MAP_SIZE, state::SHADOW_MAP_SIZE });
    ShadowMapGPInfo.DepthStencilState.depthTestEnable = VK_TRUE;
    ShadowMapGPInfo.DepthStencilState.depthWriteEnable = VK_TRUE;
    ShadowMapGPInfo.DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    State->GraphicsPipelines.ShadowMap = vtk::create_graphics_pipeline(Device->Logical, &State->RenderPasses.ShadowMap, 0, &ShadowMapGPInfo);
}

static void create_vulkan_state(state *State, vulkan_instance *VulkanInstance, assets *Assets) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    VkFormat DepthImageFormat = vtk::find_depth_image_format(Device->Physical);

    ////////////////////////////////////////////////////////////
    /// Vertex Layout
    ////////////////////////////////////////////////////////////
    State->VertexAttributeIndexes.Position = vtk::push_vertex_attribute(&State->VertexLayout, 3);
    State->VertexAttributeIndexes.Normal = vtk::push_vertex_attribute(&State->VertexLayout, 3);
    State->VertexAttributeIndexes.UV = vtk::push_vertex_attribute(&State->VertexLayout, 2);

    ////////////////////////////////////////////////////////////
    /// Uniform Buffers
    ////////////////////////////////////////////////////////////
    static const u32 UNIFORM_BUFFER_ARRAY_PADDING = 8;
    State->UniformBuffers.EntityMatrixes =
        vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, &VulkanInstance->Device, scene::MAX_ENTITIES, sizeof(matrix_ubo), Swapchain->Images.Count);
    State->UniformBuffers.ShadowMapEntityMatrixes =
        vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, &VulkanInstance->Device, scene::MAX_ENTITIES, sizeof(matrix_ubo), Swapchain->Images.Count);
    State->UniformBuffers.LightMatrixes =
        vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, &VulkanInstance->Device, scene::MAX_LIGHTS, sizeof(matrix_ubo), Swapchain->Images.Count);
    State->UniformBuffers.Lights =
        vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, &VulkanInstance->Device, 1, sizeof(State->Scene.Lights) + UNIFORM_BUFFER_ARRAY_PADDING, Swapchain->Images.Count);
    State->UniformBuffers.Materials =
        vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, &VulkanInstance->Device, state::MAX_MATERIALS, sizeof(material), Swapchain->Images.Count);

    ////////////////////////////////////////////////////////////
    /// Attachment Images
    ////////////////////////////////////////////////////////////
    vtk::image_info DepthImageInfo = {};
    DepthImageInfo.Width = Swapchain->Extent.width;
    DepthImageInfo.Height = Swapchain->Extent.height;
    DepthImageInfo.Format = DepthImageFormat;
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
    MaterialIndexImageInfo.Format = VK_FORMAT_R32_UINT;
    State->AttachmentImages.MaterialIndex = vtk::create_image(Device, &MaterialIndexImageInfo);

    ////////////////////////////////////////////////////////////
    /// Shadow Maps
    ////////////////////////////////////////////////////////////
    vtk::image_info ShadowMapInfo = {};
    ShadowMapInfo.Width = state::SHADOW_MAP_SIZE;
    ShadowMapInfo.Height = state::SHADOW_MAP_SIZE;
    ShadowMapInfo.Format = DepthImageFormat;
    ShadowMapInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    ShadowMapInfo.UsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ShadowMapInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ShadowMapInfo.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    State->ShadowMaps[0].Image = vtk::create_image(Device, &ShadowMapInfo);
    State->ShadowMaps[0].Sampler = vtk::create_sampler(Device->Logical, VK_FILTER_NEAREST);

    ////////////////////////////////////////////////////////////
    /// Sync
    ////////////////////////////////////////////////////////////
    State->Semaphores.ShadowMapFinished = vtk::create_semaphore(Device->Logical);
    State->Fences.ShadowMapFinished = vtk::create_fence(Device->Logical);

    create_descriptor_sets(State, VulkanInstance, Assets);
    create_main_render_pass(State, VulkanInstance, Assets);
    create_shadow_map_render_pass(State, VulkanInstance, Assets);
}

static void set_attenuation_values(light *Light, u32 AttenuationIndex) {
    CTK_ASSERT(AttenuationIndex < ATTENUATION_VALUE_COUNT);
    auto AttenuationValues = ATTENUATION_VALUES + AttenuationIndex;
    Light->Linear = AttenuationValues->X;
    Light->Quadratic = AttenuationValues->Y;
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
    for (u32 EntityIndex = 0; EntityIndex < EntityMap->Children.Count; ++EntityIndex) {
        ctk::data *EntityData = ctk::at(EntityMap, EntityIndex);
        entity *Entity = push_entity(Scene, EntityData->Key.Data);
        Entity->Transform = load_transform(ctk::at(EntityData, "transform"));
        Entity->TextureDS = ctk::at(&State->DescriptorSets.Textures, ctk::to_cstr(EntityData, "texture"));
        Entity->Mesh = ctk::at(&Assets->Meshes, ctk::to_cstr(EntityData, "mesh"));
        Entity->MaterialIndex = ctk::find_index(&State->Materials, ctk::to_cstr(EntityData, "material"));
        CTK_ASSERT(Entity->MaterialIndex != CTK_U32_MAX);
    }

    // Lights
    ctk::data *LightArray = ctk::at(&SceneData, "lights");
    for (u32 LightIndex = 0; LightIndex < LightArray->Children.Count; ++LightIndex) {
        ctk::data *LightData = ctk::at(LightArray, LightIndex);
        u32 AttenuationIndex = ctk::to_u32(LightData, "attenuation_index");
        transform *LightTransform = NULL;
        light *Light = push_light(Scene, AttenuationIndex, &LightTransform);
        *LightTransform = load_transform(ctk::at(LightData, "transform"));
        Light->Color = load_vec4(ctk::at(LightData, "color"));
        Light->Intensity = ctk::to_f32(LightData, "intensity");
        Light->AmbientIntensity = ctk::to_f32(LightData, "ambient_intensity");
        set_attenuation_values(Light, AttenuationIndex);
    }

    // Cleanup
    ctk::_free(&SceneData);
}

static state *create_state(vulkan_instance *VulkanInstance, assets *Assets) {
    auto State = ctk::allocate<state>();
    *State = {};
    create_test_assets(State);
    create_vulkan_state(State, VulkanInstance, Assets);
    create_scene(State, Assets, VulkanInstance);
    return State;
}

static void record_shadow_map_render_pass(vulkan_instance *VulkanInstance, state *State, u32 SwapchainImageIndex) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    scene *Scene = &State->Scene;
    vtk::render_pass *RenderPass = &State->RenderPasses.ShadowMap;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = 0;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

    VkCommandBuffer CommandBuffer = RenderPass->CommandBuffers[0];
    vtk::validate_vk_result(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                            "vkBeginCommandBuffer", "failed to begin recording command buffer");

    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = { state::SHADOW_MAP_SIZE, state::SHADOW_MAP_SIZE };

    VkRenderPassBeginInfo RenderPassBeginInfo = {};
    RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RenderPassBeginInfo.renderPass = RenderPass->Handle;
    RenderPassBeginInfo.framebuffer = RenderPass->Framebuffers[0];
    RenderPassBeginInfo.renderArea = RenderArea;
    RenderPassBeginInfo.clearValueCount = RenderPass->ClearValues.Count;
    RenderPassBeginInfo.pClearValues = RenderPass->ClearValues.Data;

    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vtk::graphics_pipeline *ShadowMapGP = &State->GraphicsPipelines.ShadowMap;
        vtk::descriptor_set *DescriptorSets[] = { &State->DescriptorSets.ShadowMapEntityMatrixes };
        for (u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex) {
            entity *Entity = Scene->Entities.Values + EntityIndex;
            mesh *Mesh = Entity->Mesh;

            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ShadowMapGP->Handle);
            vtk::bind_descriptor_sets(CommandBuffer, ShadowMapGP->Layout, 0, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets),
                                      SwapchainImageIndex, EntityIndex);
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
            vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
        }
    vkCmdEndRenderPass(CommandBuffer);
    vtk::validate_vk_result(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
}

static void record_main_render_pass(vulkan_instance *VulkanInstance, assets *Assets, state *State, u32 SwapchainImageIndex) {
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    scene *Scene = &State->Scene;
    vtk::render_pass *RenderPass = &State->RenderPasses.Main;

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
            vtk::graphics_pipeline *DeferredGP = &State->GraphicsPipelines.Deferred;
            vtk::descriptor_set *DescriptorSets[] = { &State->DescriptorSets.EntityMatrixes };
            for (u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex) {
                entity *Entity = Scene->Entities.Values + EntityIndex;
                VkDescriptorSet *TextureDS = Entity->TextureDS->Instances + 0;
                mesh *Mesh = Entity->Mesh;

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
            vtk::graphics_pipeline *LightingGP = &State->GraphicsPipelines.Lighting;
            mesh *fullscreen_quad = ctk::at(&Assets->Meshes, "fullscreen_quad");
            VkDescriptorSet DescriptorSets[] = {
                State->DescriptorSets.InputAttachments.Instances[0],
                State->DescriptorSets.ShadowMaps.Instances[0],
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
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &fullscreen_quad->VertexRegion.Buffer->Handle,
                                   &fullscreen_quad->VertexRegion.Offset);
            vkCmdBindIndexBuffer(CommandBuffer, fullscreen_quad->IndexRegion.Buffer->Handle, fullscreen_quad->IndexRegion.Offset,
                                 VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(CommandBuffer, fullscreen_quad->Indexes.Count, 1, 0, 0, 0);
        }

        vkCmdNextSubpass(CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

        // Direct Stage
        {
            // Draw Light Diamonds
            vtk::graphics_pipeline *UnlitColorGP = &State->GraphicsPipelines.UnlitColor;
            mesh *light_mesh = ctk::at(&Assets->Meshes, "direction_pyramid");
            vtk::descriptor_set *DescriptorSets[] = { &State->DescriptorSets.LightMatrixes };
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UnlitColorGP->Handle);

            // Point Lights
            for (u32 LightIndex = 0; LightIndex < Scene->Lights.Count; ++LightIndex) {
                vtk::bind_descriptor_sets(CommandBuffer, UnlitColorGP->Layout, 0, DescriptorSets, CTK_ARRAY_COUNT(DescriptorSets),
                                          SwapchainImageIndex, LightIndex);
                vkCmdPushConstants(CommandBuffer, UnlitColorGP->Layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(ctk::vec3<f32>), &Scene->Lights[LightIndex].Color);
                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &light_mesh->VertexRegion.Buffer->Handle,
                                       &light_mesh->VertexRegion.Offset);
                vkCmdBindIndexBuffer(CommandBuffer, light_mesh->IndexRegion.Buffer->Handle, light_mesh->IndexRegion.Offset,
                                     VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(CommandBuffer, light_mesh->Indexes.Count, 1, 0, 0, 0);
            }
        }
    vkCmdEndRenderPass(CommandBuffer);
    vtk::validate_vk_result(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
}

static void update_entities(VkDevice LogicalDevice, scene *Scene, glm::mat4 ViewProjectionMatrix, vtk::region *Region) {
    if (Scene->Entities.Count == 0)
        return;

    // Entity Model Matrixes
    for (u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex) {
        transform *EntityTransform = &Scene->Entities.Values[EntityIndex].Transform;
        matrix_ubo *EntityMatrixUBO = Scene->EntityMatrixUBOs + EntityIndex;
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

static glm::mat4 light_view_projection_matrix(struct transform *t) {
#if 1
    // View Matrix
    glm::vec3 light_pos = { t->Position.X, t->Position.Y, t->Position.Z };
    glm::mat4 light_trans_matrix(1.0f);
    light_trans_matrix = glm::rotate(light_trans_matrix, glm::radians(t->Rotation.X), { 1.0f, 0.0f, 0.0f });
    light_trans_matrix = glm::rotate(light_trans_matrix, glm::radians(t->Rotation.Y), { 0.0f, 1.0f, 0.0f });
    light_trans_matrix = glm::rotate(light_trans_matrix, glm::radians(t->Rotation.Z), { 0.0f, 0.0f, 1.0f });
    light_trans_matrix = glm::translate(light_trans_matrix, light_pos);
    glm::vec3 light_forward = { light_trans_matrix[0][2], light_trans_matrix[1][2], light_trans_matrix[2][2] };
    glm::mat4 view = glm::lookAt(light_pos, light_pos + light_forward, { 0.0f, -1.0f, 0.0f });

    // Projection Matrix
    glm::mat4 proj = glm::ortho(-10.0f, 10.0f, // left/right
                                -10.0f, 10.0f, // bottom/top
                                1.0f, 7.5f); // near/far
#else
    // View Matrix
    glm::vec3 pos = { t->Position.X, t->Position.Y, t->Position.Z };
    glm::mat4 matrix(1.0f);
    matrix = glm::rotate(matrix, glm::radians(t->Rotation.X), { 1.0f, 0.0f, 0.0f });
    matrix = glm::rotate(matrix, glm::radians(t->Rotation.Y), { 0.0f, 1.0f, 0.0f });
    matrix = glm::rotate(matrix, glm::radians(t->Rotation.Z), { 0.0f, 0.0f, 1.0f });
    matrix = glm::translate(matrix, pos);
    glm::vec3 forward = { matrix[0][2], matrix[1][2], matrix[2][2] };
    glm::mat4 view = glm::lookAt(pos, pos + forward, { 0.0f, -1.0f, 0.0f });

    // Projection Matrix
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1600.0f / 900.0f, 0.1f, 50.0f);
#endif
    // proj[1][1] *= -1; // Flip y value for scale (glm is designed for OpenGL).
    return proj * view;
}

static void update_lights(VkDevice LogicalDevice, state *State, glm::mat4 world_view_proj_mtx, u32 SwapchainImageIndex) {
    scene *Scene = &State->Scene;

    if (Scene->Lights.Count == 0)
        return;

    for (u32 i = 0; i < Scene->Lights.Count; ++i) {
        ctk::vec3<f32>* LightPosition = &Scene->Lights[i].Position;
        matrix_ubo *LightMatrixUBO = Scene->LightMatrixUBOs + i;
        transform *light_trans = Scene->LightTransforms + i;

        // Position
        *LightPosition = light_trans->Position;

        // World-Space Model Matrixes
        glm::mat4 model_mtx(1.0f);
        model_mtx = glm::translate(model_mtx, { LightPosition->X, LightPosition->Y, LightPosition->Z });
        model_mtx = glm::rotate(model_mtx, glm::radians(light_trans->Rotation.X), { 1.0f, 0.0f, 0.0f });
        model_mtx = glm::rotate(model_mtx, glm::radians(light_trans->Rotation.Y), { 0.0f, 1.0f, 0.0f });
        model_mtx = glm::rotate(model_mtx, glm::radians(light_trans->Rotation.Z), { 0.0f, 0.0f, 1.0f });
        model_mtx = glm::scale(model_mtx, { 0.25f, 0.25f, 0.25f });
        LightMatrixUBO->ModelMatrix = model_mtx;
        LightMatrixUBO->ModelViewProjectionMatrix = world_view_proj_mtx * model_mtx;

        // Light-Space View-Projection Matrix
        Scene->Lights[i].view_proj_mtx = light_view_projection_matrix(light_trans);
    }

    vtk::write_to_host_region(LogicalDevice, State->UniformBuffers.LightMatrixes.Regions + SwapchainImageIndex,
                              Scene->LightMatrixUBOs.Data, ctk::byte_count(&Scene->LightMatrixUBOs), 0);
    vtk::write_to_host_region(LogicalDevice, State->UniformBuffers.Lights.Regions + SwapchainImageIndex,
                              &Scene->Lights, sizeof(Scene->Lights), 0);
}

////////////////////////////////////////////////////////////
/// UI
////////////////////////////////////////////////////////////
static bool list_box_begin(cstr id, cstr title, u32 item_count, u32 viewable_item_count = -1) {
    ImGui::PushItemWidth(-1);
    char imgui_id[256] = {};
    sprintf(imgui_id, "##%s", id);
    if (title)
        ImGui::Text(title);
    return ImGui::ListBoxHeader(imgui_id, item_count, viewable_item_count);
}

static void list_box_end() {
    ImGui::ListBoxFooter();
    ImGui::PopItemWidth();
}

static bool window_begin(cstr title, s32 x, s32 y, s32 width, s32 height, s32 flags) {
    ImGui::SetNextWindowPos({ (f32)x, (f32)y });
    return ImGui::Begin(title, NULL, flags);
}

static void window_end() { ImGui::End(); }

static void separator() {
    ImVec2 output_pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(output_pos.x, output_pos.y + 1),
                                        ImVec2(output_pos.x + ImGui::GetColumnWidth(), output_pos.y + 1),
                                        IM_COL32(255, 255, 255, 64));
    ImGui::Dummy(ImVec2(0, 2));
}

static void transform_control(transform *Transform) {
    ImGui::Text("transform");
    ImGui::DragFloat3("position", &Transform->Position.X, 0.01f);
    ImGui::DragFloat3("rotation", &Transform->Rotation.X, 0.1f);
    ImGui::DragFloat3("scale", &Transform->Scale.X, 0.01f);
}

static void draw_ui(ui *UI, state *State, window *win) {
    scene *Scene = &State->Scene;
    control_state *ControlState = &State->ControlState;

    static cstr MODES[] = { "entities", "light", "materials" };
    cstr CurrentMode = MODES[ControlState->Mode];
    s32 window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove/* | ImGuiWindowFlags_NoResize*/;
    if (window_begin("states", 0, 0, 600, (s32)win->Height, window_flags)) {
        ImGui::Columns(2, NULL);

        ImGui::PushItemWidth(-1);
        if (ImGui::BeginCombo("", CurrentMode)) {
            CTK_ITERATE(CTK_ARRAY_COUNT(MODES)) {
                cstr Mode = MODES[IterationIndex];
                bool IsSelected = CurrentMode == Mode;
                if (ImGui::Selectable(Mode, IsSelected))
                    ControlState->Mode = IterationIndex;
                if (IsSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        if (ControlState->Mode == control_state::MODE_ENTITY) {
            if (list_box_begin("0", NULL, Scene->Entities.Count))
                for (u32 i = 0; i < Scene->Entities.Count; ++i)
                    if (ImGui::Selectable(Scene->Entities.Keys[i], i == ControlState->EntityIndex))
                        ControlState->EntityIndex = i;
            list_box_end();
        } else if (ControlState->Mode == control_state::MODE_LIGHT) {
            if (list_box_begin("1", NULL, Scene->Lights.Count)) {
                for (u32 i = 0; i < Scene->Lights.Count; ++i) {
                    char Name[16] = {};
                    sprintf(Name, "light %u", i);
                    if (ImGui::Selectable(Name, i == ControlState->LightIndex))
                        ControlState->LightIndex = i;
                }
            }
            list_box_end();
        } else {
            if (list_box_begin("2", NULL, State->Materials.Count))
                for (u32 i = 0; i < State->Materials.Count; ++i)
                    if (ImGui::Selectable(State->Materials.Keys[i], i == ControlState->MaterialIndex))
                        ControlState->MaterialIndex = i;
            list_box_end();
        }

        ImGui::NextColumn();
        if (ControlState->Mode == control_state::MODE_ENTITY) {
            entity *Entity = Scene->Entities.Values + ControlState->EntityIndex;
            transform_control(&Entity->Transform);
        } else if (ControlState->Mode == control_state::MODE_LIGHT) {
            light *Light = Scene->Lights + ControlState->LightIndex;
            transform_control(Scene->LightTransforms + ControlState->LightIndex);
            separator();
            ImGui::SliderFloat("intensity", &Light->Intensity, 0.0f, 1.0f);
            ImGui::SliderFloat("ambient intensity", &Light->AmbientIntensity, 0.0f, 1.0f);
            s32 *attenuation_index = (s32 *)(Scene->LightAttenuationIndexes + ControlState->LightIndex);
            if (ImGui::SliderInt("attenuation index", attenuation_index, 0, ATTENUATION_VALUE_COUNT - 1))
                set_attenuation_values(Light, *attenuation_index);
            separator();
            ImGui::Text("color");
            ImGui::ColorPicker4("##color", &Light->Color.X);
        } else if (ControlState->Mode == control_state::MODE_MATERIAL) {
            struct material *material = State->Materials.Values + ControlState->MaterialIndex;
            ImGui::SliderInt("shine exponent", (s32 *)&material->ShineExponent, 4, 1024);
        }
    }
    window_end();
}

static void controls(state *State, input_state *InputState) {
    scene *Scene = &State->Scene;
    control_state *ControlState = &State->ControlState;

    // View Mode
         if (InputState->KeyDown[GLFW_KEY_F1]) State->LightMode = state::LIGHT_MODE_COMPOSITE;
    else if (InputState->KeyDown[GLFW_KEY_F2]) State->LightMode = state::LIGHT_MODE_ALBEDO;
    else if (InputState->KeyDown[GLFW_KEY_F3]) State->LightMode = state::LIGHT_MODE_POSITION;
    else if (InputState->KeyDown[GLFW_KEY_F4]) State->LightMode = state::LIGHT_MODE_NORMAL;

    // Camera
    camera_controls(&Scene->Camera.Transform, InputState);

    // Entities/Lights
    transform *Transform = ControlState->Mode == control_state::MODE_ENTITY
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
    if (InputState->KeyDown[GLFW_KEY_L]) TransformProperty->X += Step * Modifier;
    if (InputState->KeyDown[GLFW_KEY_J]) TransformProperty->X -= Step * Modifier;
    if (InputState->KeyDown[GLFW_KEY_O]) TransformProperty->Y -= Step * Modifier;
    if (InputState->KeyDown[GLFW_KEY_U]) TransformProperty->Y += Step * Modifier;
    if (InputState->KeyDown[GLFW_KEY_I]) TransformProperty->Z += Step * Modifier;
    if (InputState->KeyDown[GLFW_KEY_K]) TransformProperty->Z -= Step * Modifier;
}

static void test_main() {
    App app = {};

    app.InputState = ctk::allocate<input_state>(1);
    *app.InputState = {};

    WindowInfo window_info = {};
    window_info.user_pointer = (void *)&app;
    window_info.key_callback = key_callback;
    window_info.mouse_button_callback = mouse_button_callback;
    app.Window = create_window(&window_info);

    app.VulkanInstance = create_vulkan_instance(app.Window);
    app.Assets = create_assets(app.VulkanInstance);
    app.State = create_state(app.VulkanInstance, app.Assets);

    app.UI = create_ui(app.Window, app.VulkanInstance);
    app.UI->IO->KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    app.UI->IO->KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;

    // Main Loop
    vtk::device *Device = &app.VulkanInstance->Device;
    vtk::swapchain *Swapchain = &app.VulkanInstance->Swapchain;
    while (!glfwWindowShouldClose(app.Window->Handle)) {
        ////////////////////////////////////////////////////////////
        /// Input
        ////////////////////////////////////////////////////////////
        glfwPollEvents();
        if (app.InputState->KeyDown[GLFW_KEY_ESCAPE])
            break;
        if (!app.UI->IO->WantCaptureKeyboard) {
            update_input_state(app.InputState, app.Window->Handle);
            controls(app.State, app.InputState);
        }

        ////////////////////////////////////////////////////////////
        /// Frame
        ////////////////////////////////////////////////////////////
        u32 SwapchainImageIndex = aquire_next_swapchain_image_index(app.VulkanInstance);
        synchronize_current_frame(app.VulkanInstance, SwapchainImageIndex);

        ////////////////////////////////////////////////////////////
        /// Render Pass Command Buffer Recording
        ////////////////////////////////////////////////////////////

        // Update scene rendering data.
        glm::mat4 cam_view_proj_mtx = camera_view_projection_matrix(&app.State->Scene.Camera);
        update_entities(Device->Logical, &app.State->Scene, light_view_projection_matrix(app.State->Scene.LightTransforms + 0),
                        app.State->UniformBuffers.ShadowMapEntityMatrixes.Regions + SwapchainImageIndex);
        update_entities(Device->Logical, &app.State->Scene, cam_view_proj_mtx,
                        app.State->UniformBuffers.EntityMatrixes.Regions + SwapchainImageIndex);
        update_lights(Device->Logical, app.State, cam_view_proj_mtx, SwapchainImageIndex);
        vtk::write_to_host_region(Device->Logical, app.State->UniformBuffers.Materials.Regions + SwapchainImageIndex,
                                  app.State->Materials.Values, ctk::values_byte_count(&app.State->Materials), 0);
        ui_new_frame();
        draw_ui(app.UI, app.State, app.Window);

        // Shadow Map Rendering
        // Ensure command buffer is finished rendering before recording new shadow map render pass commands.
        vkWaitForFences(Device->Logical, 1, &app.State->Fences.ShadowMapFinished, VK_TRUE, UINT64_MAX);
        vkResetFences(Device->Logical, 1, &app.State->Fences.ShadowMapFinished);
        record_shadow_map_render_pass(app.VulkanInstance, app.State, SwapchainImageIndex);

        // Scene Rendering
        record_main_render_pass(app.VulkanInstance, app.Assets, app.State, SwapchainImageIndex);

        // UI Rendering
        record_ui_render_pass(app.VulkanInstance, app.UI, SwapchainImageIndex);

        ////////////////////////////////////////////////////////////
        /// Command Buffers Submission
        ////////////////////////////////////////////////////////////
        vtk::frame_state *FrameState = &app.VulkanInstance->FrameState;
        vtk::frame *CurrentFrame = FrameState->Frames + FrameState->CurrentFrameIndex;

        // Shadow Map Command Buffer
        {
            VkSubmitInfo submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.waitSemaphoreCount = 0;
            submit_info.pWaitSemaphores = NULL;
            submit_info.pWaitDstStageMask = NULL;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = app.State->RenderPasses.ShadowMap.CommandBuffers + 0;
            submit_info.signalSemaphoreCount = 1;
            submit_info.pSignalSemaphores = &app.State->Semaphores.ShadowMapFinished;
            vtk::validate_vk_result(vkQueueSubmit(Device->GraphicsQueue, 1, &submit_info, app.State->Fences.ShadowMapFinished),
                                    "vkQueueSubmit", "failed to submit command buffer to graphics queue");
        }

        // Render Command Buffers
        {
            VkCommandBuffer cmd_bufs[] = {
                app.State->RenderPasses.Main.CommandBuffers[SwapchainImageIndex],
                app.UI->RenderPass.CommandBuffers[SwapchainImageIndex],
            };

            // 1:1 semaphores:wait_stages
            VkSemaphore wait_semaphores[] = {
                app.State->Semaphores.ShadowMapFinished,
                CurrentFrame->ImageAquiredSemaphore,
            };
            VkPipelineStageFlags wait_stages[] = {
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            };

            VkSubmitInfo submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.waitSemaphoreCount = CTK_ARRAY_COUNT(wait_semaphores);
            submit_info.pWaitSemaphores = wait_semaphores;
            submit_info.pWaitDstStageMask = wait_stages;
            submit_info.commandBufferCount = CTK_ARRAY_COUNT(cmd_bufs);
            submit_info.pCommandBuffers = cmd_bufs;
            submit_info.signalSemaphoreCount = 1;
            submit_info.pSignalSemaphores = &CurrentFrame->RenderFinishedSemaphore;
            vtk::validate_vk_result(vkQueueSubmit(Device->GraphicsQueue, 1, &submit_info, CurrentFrame->InFlightFence),
                                    "vkQueueSubmit", "failed to submit command buffer to graphics queue");
        }

        ////////////////////////////////////////////////////////////
        /// Presentation
        ////////////////////////////////////////////////////////////

        // Provide 1:1 index per swapchain.
        VkSwapchainKHR Swapchains[] = { Swapchain->Handle };
        u32 SwapchainImageIndexes[] = { SwapchainImageIndex };
        VkSemaphore PresentWaitSemaphores[] = { CurrentFrame->RenderFinishedSemaphore };

        VkPresentInfoKHR PresentInfo = {};
        PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        PresentInfo.waitSemaphoreCount = CTK_ARRAY_COUNT(PresentWaitSemaphores);
        PresentInfo.pWaitSemaphores = PresentWaitSemaphores;
        PresentInfo.swapchainCount = CTK_ARRAY_COUNT(Swapchains);
        PresentInfo.pSwapchains = Swapchains;
        PresentInfo.pImageIndices = SwapchainImageIndexes;
        PresentInfo.pResults = NULL;

        // Submit Swapchains to present queue for presentation once rendering is complete.
        vtk::validate_vk_result(vkQueuePresentKHR(Device->PresentQueue, &PresentInfo), "vkQueuePresentKHR",
                                "failed to queue image for presentation");

        cycle_frame(app.VulkanInstance);

        Sleep(1);
    }
}
