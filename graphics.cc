#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "gfx/graphics.h"
#include "ctk/data.h"

////////////////////////////////////////////////////////////
/// Internal
////////////////////////////////////////////////////////////
static void
error_callback(s32 Error, cstr Description)
{
    CTK_FATAL("[%d] %s", Error, Description)
}

static void
key_callback(GLFWwindow *Window, s32 Key, s32 Scancode, s32 Action, s32 Mods)
{
    auto InputState = (input_state *)glfwGetWindowUserPointer(Window);
    InputState->KeyDown[Key] = Action == GLFW_PRESS || Action == GLFW_REPEAT;
}

static void
mouse_button_callback(GLFWwindow *Window, s32 button, s32 Action, s32 Mods)
{
    auto InputState = (input_state *)glfwGetWindowUserPointer(Window);
    InputState->MouseButtonDown[button] = Action == GLFW_PRESS || Action == GLFW_REPEAT;
}

static mesh
create_mesh(vulkan_instance *VulkanInstance, cstr Path)
{
    mesh Mesh = {};

    vtk::device *Device = &VulkanInstance->Device;
    VkCommandPool GraphicsCommandPool = VulkanInstance->GraphicsCommandPool;
    vtk::buffer *DeviceBuffer = &VulkanInstance->DeviceBuffer;
    vtk::region *StagingRegion = &VulkanInstance->StagingRegion;

    u32 ProcessingFlags = aiProcess_CalcTangentSpace |
                          aiProcess_Triangulate |
                          aiProcess_JoinIdenticalVertices |
                          aiProcess_SortByPType;
    const aiScene *Scene = aiImportFile(Path, ProcessingFlags);
    if(Scene == NULL || Scene->mRootNode == NULL || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
    {
        CTK_FATAL("error loading mesh from path \"%s\": %s", Path, aiGetErrorString())
    }

    ////////////////////////////////////////////////////////////
    /// Allocation
    ////////////////////////////////////////////////////////////
    ctk::todo("HACK: Baking all meshes from file into single mesh.");
    u32 VertexCount = 0;
    u32 IndexCount = 0;
    for(u32 MeshIndex = 0; MeshIndex < Scene->mNumMeshes; ++MeshIndex)
    {
        aiMesh *Mesh = Scene->mMeshes[MeshIndex];
        VertexCount += Mesh->mNumVertices;
        for(u32 FaceIndex = 0; FaceIndex < Mesh->mNumFaces; ++FaceIndex)
        {
            IndexCount += Mesh->mFaces[FaceIndex].mNumIndices;
        }
    }
    Mesh.Vertexes = ctk::create_array_empty<vertex>(VertexCount);
    Mesh.Indexes = ctk::create_array_empty<u32>(IndexCount);

    ////////////////////////////////////////////////////////////
    /// Processing
    ////////////////////////////////////////////////////////////
    for(u32 MeshIndex = 0; MeshIndex < Scene->mNumMeshes; ++MeshIndex)
    {
        aiMesh *SceneMesh = Scene->mMeshes[MeshIndex];
        u32 IndexBase = Mesh.Vertexes.Count;
        for(u32 VertexIndex = 0; VertexIndex < SceneMesh->mNumVertices; ++VertexIndex)
        {
            vertex *Vertex = ctk::push(&Mesh.Vertexes);
            aiVector3D *Position = SceneMesh->mVertices + VertexIndex;
            aiVector3D *Normal = SceneMesh->mNormals + VertexIndex;
            Vertex->Position = { Position->x, Position->y, Position->z };
            Vertex->Normal = { Normal->x, Normal->y, Normal->z };

            // Texture coordinates are optional.
            if(SceneMesh->mTextureCoords[0] == NULL)
            {
                Vertex->UV = { 0, 0 };
            }
            else
            {
                aiVector3D *UV = SceneMesh->mTextureCoords[0] + VertexIndex;
                Vertex->UV = { UV->x, 1 - UV->y }; // Blender's UV y-axis is inverse from Vulkan's.
            }
        }
        for(u32 FaceIndex = 0; FaceIndex < SceneMesh->mNumFaces; ++FaceIndex)
        {
            aiFace *Face = SceneMesh->mFaces + FaceIndex;
            for(u32 IndexIndex = 0; IndexIndex < Face->mNumIndices; ++IndexIndex)
            {
                ctk::push(&Mesh.Indexes, IndexBase + Face->mIndices[IndexIndex]);
            }
        }
    }

    // Allocate and write vertex/index data to their associated regions.
    u32 VertexByteSize = ctk::byte_size(&Mesh.Vertexes);
    u32 IndexByteSize = ctk::byte_size(&Mesh.Indexes);
    Mesh.VertexRegion = vtk::allocate_region(DeviceBuffer, VertexByteSize);
    Mesh.IndexRegion = vtk::allocate_region(DeviceBuffer, IndexByteSize);
    vtk::write_to_device_region(Device, GraphicsCommandPool, StagingRegion, &Mesh.VertexRegion,
                                Mesh.Vertexes.Data, VertexByteSize, 0);
    vtk::write_to_device_region(Device, GraphicsCommandPool, StagingRegion, &Mesh.IndexRegion,
                                Mesh.Indexes.Data, IndexByteSize, 0);

    // Cleanup
    aiReleaseImport(Scene);

    return Mesh;
}

static void
create_render_passes(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState)
{
    static const VkClearValue COLOR_ATTACHMENT_CLEAR_VALUE = { 0.04f, 0.04f, 0.04f, 1.0f };
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::image *DepthImage = &VulkanState->DepthImage;

    // Depth Attachment
    vtk::attachment DepthAttachment = {};
    DepthAttachment.Description.format = DepthImage->Format;
    DepthAttachment.Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment.Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment.Description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment.Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    DepthAttachment.Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment.Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment.Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    DepthAttachment.ClearValue = { 1.0f, 0 };

    ////////////////////////////////////////////////////////////
    /// Direct Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass *DirectRenderPass = ctk::push(&VulkanState->RenderPasses, "direct");

    // Render Pass Info
    vtk::render_pass_info DirectRenderPassInfo = {};

    // Attachments
    u32 DirectColorAttachmentIndex = DirectRenderPassInfo.Attachments.Count;
    vtk::attachment *DirectColorAttachment = ctk::push(&DirectRenderPassInfo.Attachments);
    DirectColorAttachment->Description.format = Swapchain->ImageFormat;
    DirectColorAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DirectColorAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DirectColorAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    DirectColorAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    DirectColorAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DirectColorAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DirectColorAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    DirectColorAttachment->ClearValue = COLOR_ATTACHMENT_CLEAR_VALUE;

    u32 DirectDepthAttachmentIndex = DirectRenderPassInfo.Attachments.Count;
    ctk::push(&DirectRenderPassInfo.Attachments, DepthAttachment);

    // Subpasses
    vtk::subpass *DirectSubpass = ctk::push(&DirectRenderPassInfo.Subpasses);

    VkAttachmentReference *DirectColorAttachmentReference = ctk::push(&DirectSubpass->ColorAttachmentReferences);
    DirectColorAttachmentReference->attachment = DirectColorAttachmentIndex;
    DirectColorAttachmentReference->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    DirectSubpass->DepthAttachmentReference.Set = true;
    DirectSubpass->DepthAttachmentReference.Value.attachment = DirectDepthAttachmentIndex;
    DirectSubpass->DepthAttachmentReference.Value.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Framebuffer Infos
    for(u32 FramebufferIndex = 0; FramebufferIndex < Swapchain->Images.Count; ++FramebufferIndex)
    {
        vtk::framebuffer_info *FramebufferInfo = ctk::push(&DirectRenderPassInfo.FramebufferInfos);
        ctk::push(&FramebufferInfo->Attachments, Swapchain->Images[FramebufferIndex].View);
        ctk::push(&FramebufferInfo->Attachments, DepthImage->View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    *DirectRenderPass = vtk::create_render_pass(Device->Logical, VulkanInstance->GraphicsCommandPool, &DirectRenderPassInfo);
}

////////////////////////////////////////////////////////////
/// Interface
////////////////////////////////////////////////////////////
window *
create_window(input_state *InputState)
{
    auto Window = ctk::allocate<window>();
    *Window = {};
    ctk::data Data = ctk::load_data("assets/data/window.ctkd");
    glfwSetErrorCallback(error_callback);
    if(glfwInit() != GLFW_TRUE)
    {
        CTK_FATAL("failed to init GLFW")
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    Window->Width = ctk::to_s32(&Data, "width");
    Window->Height = ctk::to_s32(&Data, "height");
    Window->Handle = glfwCreateWindow(Window->Width, Window->Height, ctk::to_cstr(&Data, "title"), NULL, NULL);
    if(Window->Handle == NULL)
    {
        CTK_FATAL("failed to create window")
    }
    glfwSetWindowPos(Window->Handle, ctk::to_s32(&Data, "x"), ctk::to_s32(&Data, "y"));
    glfwSetWindowUserPointer(Window->Handle, (void *)InputState);
    // glfwSetFramebufferSizeCallback(Window->Handle, FramebufferResizeCallback);
    glfwSetKeyCallback(Window->Handle, key_callback);
    glfwSetMouseButtonCallback(Window->Handle, mouse_button_callback);
    return Window;
}

vulkan_instance *
create_vulkan_instance(window *Window)
{
    auto VulkanInstance = ctk::allocate<vulkan_instance>();
    *VulkanInstance = {};

    vtk::instance *Instance = &VulkanInstance->Instance;
    VkSurfaceKHR *PlatformSurface = &VulkanInstance->PlatformSurface;
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    VkCommandPool *GraphicsCommandPool = &VulkanInstance->GraphicsCommandPool;
    vtk::buffer *HostBuffer = &VulkanInstance->HostBuffer;

    ctk::data VulkanInstanceData = ctk::load_data("assets/data/vulkan_instance.ctkd");

    // Instance
    u32 GLFWExtensionCount = 0;
    cstr *GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionCount);
    vtk::instance_info InstanceInfo = {};
    ctk::push(&InstanceInfo.Extensions, GLFWExtensions, GLFWExtensionCount);
    InstanceInfo.Debug = ctk::to_b32(&VulkanInstanceData, "debug");
    InstanceInfo.AppName = ctk::to_cstr(&VulkanInstanceData, "app_name");
    *Instance = vtk::create_instance(&InstanceInfo);

    // Platform Surface
    vtk::validate_vk_result(glfwCreateWindowSurface(Instance->Handle, Window->Handle, NULL, PlatformSurface),
                            "glfwCreateWindowSurface", "failed to create GLFW surface");

    // Device
    vtk::device_info DeviceInfo = {};
    ctk::push(&DeviceInfo.Extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME); // Swapchains required for rendering.
    DeviceInfo.Features.geometryShader = VK_TRUE;
    DeviceInfo.Features.samplerAnisotropy = VK_TRUE;
    // DeviceInfo.Features.vertexPipelineStoresAndAtomics = VK_TRUE;
    *Device = vtk::create_device(Instance->Handle, *PlatformSurface, &DeviceInfo);

    // Swapchain
    *Swapchain = vtk::create_swapchain(Device, *PlatformSurface);

    // Graphics Command Pool
    *GraphicsCommandPool = vtk::create_command_pool(Device->Logical, Device->QueueFamilyIndexes.Graphics);

    // Frame State
    VulkanInstance->FrameState = vtk::create_frame_state(Device->Logical, 2, Swapchain->Images.Count);

    // Buffers
    vtk::buffer_info HostBufferInfo = {};
    HostBufferInfo.Size = 100 * CTK_MEGABYTE;
    HostBufferInfo.UsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    HostBufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    *HostBuffer = vtk::create_buffer(Device, &HostBufferInfo);

    vtk::buffer_info DeviceBufferInfo = {};
    DeviceBufferInfo.Size = 100 * CTK_MEGABYTE;
    DeviceBufferInfo.UsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    DeviceBufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VulkanInstance->DeviceBuffer = vtk::create_buffer(Device, &DeviceBufferInfo);

    // Regions
    VulkanInstance->StagingRegion = vtk::allocate_region(HostBuffer, 50 * CTK_MEGABYTE);

    return VulkanInstance;
}

assets *
create_assets(vulkan_instance *VulkanInstance)
{
    auto Assets = ctk::allocate<assets>();
    *Assets = {};

    vtk::device *Device = &VulkanInstance->Device;

    ctk::data AssetData = ctk::load_data("assets/data/assets.ctkd");

    ////////////////////////////////////////////////////////////
    /// Textures
    ////////////////////////////////////////////////////////////
    ctk::data *TextureMap = ctk::at(&AssetData, "textures");
    for(u32 TextureIndex = 0; TextureIndex < TextureMap->Children.Count; ++TextureIndex)
    {
        ctk::data *TextureData = ctk::at(TextureMap, TextureIndex);
        vtk::texture_info TextureInfo = {};
        TextureInfo.Filter = vtk::get_vk_filter(ctk::to_cstr(TextureData, "filter"));
        ctk::push(&Assets->Textures, TextureData->Key.Data,
                  vtk::create_texture(Device, VulkanInstance->GraphicsCommandPool, &VulkanInstance->StagingRegion,
                                      ctk::to_cstr(TextureData, "path"), &TextureInfo));
    }

    ////////////////////////////////////////////////////////////
    /// Shader Modules
    ////////////////////////////////////////////////////////////
    ctk::data *ShaderModuleMap = ctk::at(&AssetData, "shader_modules");
    for(u32 ShaderModuleIndex = 0; ShaderModuleIndex < ShaderModuleMap->Children.Count; ++ShaderModuleIndex)
    {
        ctk::data *ShaderModuleData = ctk::at(ShaderModuleMap, ShaderModuleIndex);
        VkShaderStageFlagBits Stage = vtk::get_vk_shader_stage_flag_bits(ctk::to_cstr(ShaderModuleData, "stage"));
        ctk::push(&Assets->ShaderModules, ShaderModuleData->Key.Data,
                  vtk::create_shader_module(Device->Logical, ctk::to_cstr(ShaderModuleData, "path"), Stage));
    }

    ////////////////////////////////////////////////////////////
    /// Meshes
    ////////////////////////////////////////////////////////////
    ctk::data *ModelMap = ctk::at(&AssetData, "models");
    for(u32 ModelIndex = 0; ModelIndex < ModelMap->Children.Count; ++ModelIndex)
    {
        ctk::data *ModelData = ctk::at(ModelMap, ModelIndex);
        ctk::push(&Assets->Meshes, ModelData->Key.Data, create_mesh(VulkanInstance, ctk::to_cstr(ModelData, "path")));
    }

    return Assets;
}

vulkan_state *
create_vulkan_state(vulkan_instance *VulkanInstance, assets *Assets)
{
    auto VulkanState = ctk::allocate<vulkan_state>();
    *VulkanState = {};

    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;

    VkDescriptorPool *DescriptorPool = &VulkanState->DescriptorPool;
    vtk::vertex_layout *VertexLayout = &VulkanState->VertexLayout;
    auto *UniformBuffers = &VulkanState->UniformBuffers;
    auto *DescriptorSets = &VulkanState->DescriptorSets;
    auto *VertexAttributeIndexes = &VulkanState->VertexAttributeIndexes;

    ctk::data VulkanStateData = ctk::load_data("assets/data/vulkan_state.ctkd");

    ////////////////////////////////////////////////////////////
    /// Predefined State
    ////////////////////////////////////////////////////////////
    ctk::todo("using frame count instead of swapchain image count");

    // Uniform Buffers
    ctk::push(UniformBuffers, "entity",
              vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, 64, sizeof(entity_ubo), FrameState->Frames.Count));
    ctk::push(UniformBuffers, "light",
              vtk::create_uniform_buffer(&VulkanInstance->HostBuffer, 64, sizeof(light_ubo), FrameState->Frames.Count));

    // Depth Image
    vtk::image_info DepthImageInfo = {};
    DepthImageInfo.Width = Swapchain->Extent.width;
    DepthImageInfo.Height = Swapchain->Extent.height;
    DepthImageInfo.Format = vtk::find_depth_image_format(Device->Physical);
    DepthImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    DepthImageInfo.UsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    DepthImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    DepthImageInfo.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    VulkanState->DepthImage = vtk::create_image(Device, &DepthImageInfo);

    // Render Passes
    create_render_passes(VulkanInstance, Assets, VulkanState);

    ////////////////////////////////////////////////////////////
    /// Descriptor Sets
    ////////////////////////////////////////////////////////////

    // Descriptor Infos
    ctk::smap<vtk::descriptor_info, 8> DescriptorInfos = {};
    ctk::data *DescriptorMap = ctk::at(&VulkanStateData, "descriptors");
    for(u32 DescriptorIndex = 0; DescriptorIndex < DescriptorMap->Children.Count; ++DescriptorIndex)
    {
        ctk::data *DescriptorData = ctk::at(DescriptorMap, DescriptorIndex);
        ctk::data *ShaderStageFlagsArray = ctk::at(DescriptorData, "shader_stage_flags");
        VkDescriptorType Type = vtk::get_vk_descriptor_type(ctk::to_cstr(DescriptorData, "type"));
        VkShaderStageFlags ShaderStageFlags = 0;
        for(u32 ShaderStageIndex = 0; ShaderStageIndex < ShaderStageFlagsArray->Children.Count; ++ShaderStageIndex)
        {
            ShaderStageFlags |= vtk::get_vk_shader_stage_flag_bits(ctk::to_cstr(ShaderStageFlagsArray, ShaderStageIndex));
        }

        vtk::descriptor_info* DescriptorInfo = ctk::push(&DescriptorInfos, DescriptorData->Key.Data);
        DescriptorInfo->Type = Type;
        DescriptorInfo->ShaderStageFlags = ShaderStageFlags;
        DescriptorInfo->Count = ctk::to_u32(DescriptorData, "count");
        if(Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
           Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        {
            DescriptorInfo->UniformBuffer = ctk::at(UniformBuffers, ctk::to_cstr(DescriptorData, "uniform_buffer"));
        }
        else if(Type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            DescriptorInfo->Texture = ctk::at(&Assets->Textures, ctk::to_cstr(DescriptorData, "texture"));
        }
        else
        {
            CTK_FATAL("unhandled descriptor type when loading descriptors")
        }
    }

    // Descriptor Set Infos
    ctk::smap<vtk::descriptor_set_info, 8> DescriptorSetInfos = {};
    ctk::data *DescriptorSetMap = ctk::at(&VulkanStateData, "descriptor_sets");
    for(u32 DescriptorSetIndex = 0; DescriptorSetIndex < DescriptorSetMap->Children.Count; ++DescriptorSetIndex)
    {
        ctk::data *DescriptorSetData = ctk::at(DescriptorSetMap, DescriptorSetIndex);
        vtk::descriptor_set_info *DescriptorSetInfo = ctk::push(&DescriptorSetInfos, DescriptorSetData->Key.Data);
        b32 IsDynamic = ctk::equal(ctk::to_cstr(DescriptorSetData, "type"), "dynamic");
        ctk::todo("using frame count instead of swapchain image count");
        DescriptorSetInfo->InstanceCount = IsDynamic ? FrameState->Frames.Count : 1;
        ctk::data *DescriptorBindingArray = ctk::at(DescriptorSetData, "descriptor_bindings");
        for(u32 DescriptorBindingIndex = 0; DescriptorBindingIndex < DescriptorBindingArray->Children.Count; ++DescriptorBindingIndex)
        {
            ctk::data *DescriptorBindingData = ctk::at(DescriptorBindingArray, DescriptorBindingIndex);
            ctk::push(&DescriptorSetInfo->DescriptorBindings,
                      {
                          ctk::to_u32(DescriptorBindingData, "binding"),
                          ctk::at(&DescriptorInfos, ctk::to_cstr(DescriptorBindingData, "descriptor")),
                      });
        }
    }

    // Pool
    *DescriptorPool = vtk::create_descriptor_pool(Device->Logical, DescriptorSetInfos.Values, DescriptorSetInfos.Count);

    // Mirror descriptor set infos map to store descriptor sets.
    for(u32 DescriptorSetIndex = 0; DescriptorSetIndex < DescriptorSetInfos.Count; ++DescriptorSetIndex)
    {
        ctk::push(DescriptorSets, DescriptorSetInfos.Keys[DescriptorSetIndex]);
    }
    vtk::create_descriptor_sets(Device->Logical, *DescriptorPool, DescriptorSetInfos.Values, DescriptorSetInfos.Count, DescriptorSets->Values);

    ////////////////////////////////////////////////////////////
    /// Vertex Layout
    ////////////////////////////////////////////////////////////
    ctk::data *VertexLayoutMap = ctk::at(&VulkanStateData, "vertex_layout");
    for(u32 VertexAttributeIndex = 0; VertexAttributeIndex < VertexLayoutMap->Children.Count; ++VertexAttributeIndex)
    {
        ctk::data *VertexAttributeData = ctk::at(VertexLayoutMap, VertexAttributeIndex);
        ctk::push(VertexAttributeIndexes, VertexAttributeData->Key.Data,
                  vtk::push_vertex_attribute(VertexLayout, ctk::to_u32(VertexAttributeData, "element_count")));
    }

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////
    ctk::data *GraphicsPipelineMap = ctk::at(&VulkanStateData, "graphics_pipelines");
    for(u32 GraphicsPipelineIndex = 0; GraphicsPipelineIndex < GraphicsPipelineMap->Children.Count; ++GraphicsPipelineIndex)
    {
        ctk::data *GraphicsPipelineData = ctk::at(GraphicsPipelineMap, GraphicsPipelineIndex);
        ctk::data *ShaderModuleArray = ctk::at(GraphicsPipelineData, "shader_modules");
        ctk::data *DescriptorSetLayoutArray = ctk::at(GraphicsPipelineData, "descriptor_set_layouts");
        ctk::data *VertexInputArray = ctk::at(GraphicsPipelineData, "vertex_inputs");
        vtk::graphics_pipeline_info GraphicsPipelineInfo = {};
        for(u32 ShaderModuleIndex = 0; ShaderModuleIndex < ShaderModuleArray->Children.Count; ++ShaderModuleIndex)
        {
            ctk::push(&GraphicsPipelineInfo.ShaderModules,
                      ctk::at(&Assets->ShaderModules, ctk::to_cstr(ShaderModuleArray, ShaderModuleIndex)));
        }
        for(u32 DescriptorSetLayoutIndex = 0; DescriptorSetLayoutIndex < DescriptorSetLayoutArray->Children.Count;
            ++DescriptorSetLayoutIndex)
        {
            ctk::push(&GraphicsPipelineInfo.DescriptorSetLayouts,
                      ctk::at(DescriptorSets, ctk::to_cstr(DescriptorSetLayoutArray, DescriptorSetLayoutIndex))->Layout);
        }
        for(u32 VertexInputIndex = 0; VertexInputIndex < VertexInputArray->Children.Count; ++VertexInputIndex)
        {
            ctk::data *VertexInputData = ctk::at(VertexInputArray, VertexInputIndex);
            ctk::push(&GraphicsPipelineInfo.VertexInputs,
                      {
                          ctk::to_u32(VertexInputData, "binding"),
                          ctk::to_u32(VertexInputData, "location"),
                          *ctk::at(VertexAttributeIndexes, ctk::to_cstr(VertexInputData, "attribute"))
                      });
        }
        GraphicsPipelineInfo.VertexLayout = VertexLayout;
        GraphicsPipelineInfo.ViewportExtent = Swapchain->Extent;
        GraphicsPipelineInfo.PrimitiveTopology = vtk::get_vk_primitive_topology(ctk::to_cstr(GraphicsPipelineData, "primitive_topology"));
        GraphicsPipelineInfo.DepthTesting = vtk::get_vk_bool_32(ctk::to_cstr(GraphicsPipelineData, "depth_testing"));
        vtk::render_pass *RenderPass = ctk::at(&VulkanState->RenderPasses, ctk::to_cstr(GraphicsPipelineData, "render_pass"));
        ctk::push(&VulkanState->GraphicsPipelines, GraphicsPipelineData->Key.Data,
                  vtk::create_graphics_pipeline(Device->Logical, RenderPass, &GraphicsPipelineInfo));
    }

    return VulkanState;
}

void
update_uniform_data(vulkan_instance *VulkanInstance, scene *Scene)
{
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    transform *CameraTransform = &Scene->Camera.Transform;

    // View Matrix
    glm::vec3 CameraPosition = { CameraTransform->Position.X, CameraTransform->Position.Y, CameraTransform->Position.Z };
    glm::mat4 CameraMatrix(1.0f);
    CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraTransform->Rotation.X), { 1.0f, 0.0f, 0.0f });
    CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraTransform->Rotation.Y), { 0.0f, 1.0f, 0.0f });
    CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraTransform->Rotation.Z), { 0.0f, 0.0f, 1.0f });
    CameraMatrix = glm::translate(CameraMatrix, CameraPosition);
    glm::vec3 CameraForward = { CameraMatrix[0][2], CameraMatrix[1][2], CameraMatrix[2][2] };
    glm::mat4 ViewMatrix = glm::lookAt(CameraPosition, CameraPosition + CameraForward, { 0.0f, -1.0f, 0.0f });

    // Projection Matrix
    f32 Aspect = Swapchain->Extent.width / (f32)Swapchain->Extent.height;
    glm::mat4 ProjectionMatrix = glm::perspective(glm::radians(Scene->Camera.FieldOfView), Aspect, 0.1f, 1000.0f);
    ProjectionMatrix[1][1] *= -1; // Flip y value for scale (glm is designed for OpenGL).

    // Entity Model Matrixes
    for(u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex)
    {
        transform *EntityTransform = &Scene->Entities.Values[EntityIndex].Transform;
        glm::mat4 ModelMatrix(1.0f);
        ModelMatrix = glm::translate(ModelMatrix, { EntityTransform->Position.X, EntityTransform->Position.Y, EntityTransform->Position.Z });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(EntityTransform->Rotation.X), { 1.0f, 0.0f, 0.0f });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(EntityTransform->Rotation.Y), { 0.0f, 1.0f, 0.0f });
        ModelMatrix = glm::rotate(ModelMatrix, glm::radians(EntityTransform->Rotation.Z), { 0.0f, 0.0f, 1.0f });
        ModelMatrix = glm::scale(ModelMatrix, { EntityTransform->Scale.X, EntityTransform->Scale.Y, EntityTransform->Scale.Z });
        Scene->EntityUBOs[EntityIndex].ModelMatrix = ModelMatrix;
        Scene->EntityUBOs[EntityIndex].MVPMatrix = ProjectionMatrix * ViewMatrix * ModelMatrix;
    }

    // Write all entity ubos to current frame's entity uniform buffer region.
    vtk::region *EntityUniformBufferRegion = Scene->EntityUniformBuffer->Regions + VulkanInstance->FrameState.CurrentFrameIndex;
    vtk::write_to_host_region(VulkanInstance->Device.Logical, EntityUniformBufferRegion,
                              Scene->EntityUBOs.Data, ctk::byte_count(&Scene->EntityUBOs), 0);
}

void
record_direct_render_pass(vulkan_instance *VulkanInstance, vulkan_state *VulkanState, scene *Scene)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;
    vtk::render_pass *DirectRenderPass = ctk::at(&VulkanState->RenderPasses, "direct");

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
        VkCommandBuffer CommandBuffer = *ctk::at(&DirectRenderPass->CommandBuffers, SwapchainImageIndex);
        vtk::validate_vk_result(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                                "vkBeginCommandBuffer", "failed to begin recording command buffer");
        VkRenderPassBeginInfo RenderPassBeginInfo = {};
        RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassBeginInfo.renderPass = DirectRenderPass->Handle;
        RenderPassBeginInfo.framebuffer = *ctk::at(&DirectRenderPass->Framebuffers, SwapchainImageIndex);
        RenderPassBeginInfo.renderArea = RenderArea;
        RenderPassBeginInfo.clearValueCount = DirectRenderPass->ClearValues.Count;
        RenderPassBeginInfo.pClearValues = DirectRenderPass->ClearValues.Data;

        // Begin
        vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        ////////////////////////////////////////////////////////////
        /// Render Commands
        ////////////////////////////////////////////////////////////
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

        // End
        vkCmdEndRenderPass(CommandBuffer);
        vtk::validate_vk_result(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
    }
}

void
render(vulkan_instance *VulkanInstance, vulkan_state *VulkanState)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;

    vtk::frame *CurrentFrame = FrameState->Frames + FrameState->CurrentFrameIndex;

    // Wait on current frame's fence if still unsignaled.
    vkWaitForFences(Device->Logical, 1, &CurrentFrame->InFlightFence, VK_TRUE, UINT64_MAX);

    // Aquire next swapchain image index, using a semaphore to signal when image is available for rendering.
    u32 SwapchainImageIndex = VTK_UNSET_INDEX;
    {
        VkResult Result = vkAcquireNextImageKHR(Device->Logical, Swapchain->Handle, UINT64_MAX, CurrentFrame->ImageAquiredSemaphore,
                                                VK_NULL_HANDLE, &SwapchainImageIndex);
        vtk::validate_vk_result(Result, "vkAcquireNextImageKHR", "failed to aquire next swapchain image");
    }

    // Wait on swapchain images previously associated frame fence before rendering.
    VkFence *PreviousFrameInFlightFence = FrameState->PreviousFrameInFlightFences + SwapchainImageIndex;
    if(*PreviousFrameInFlightFence != VK_NULL_HANDLE)
    {
        vkWaitForFences(Device->Logical, 1, PreviousFrameInFlightFence, VK_TRUE, UINT64_MAX);
    }
    vkResetFences(Device->Logical, 1, &CurrentFrame->InFlightFence);
    *PreviousFrameInFlightFence = CurrentFrame->InFlightFence;

    ////////////////////////////////////////////////////////////
    /// Command Buffers Submission
    ////////////////////////////////////////////////////////////
    VkSemaphore DirectWaitSemaphores[] = { CurrentFrame->ImageAquiredSemaphore };
    VkPipelineStageFlags DirectWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore DirectSignalSemaphores[] = { CurrentFrame->RenderFinishedSemaphore };
    VkCommandBuffer DirectCommandBuffers[] =
    {
        *ctk::at(&ctk::at(&VulkanState->RenderPasses, "direct")->CommandBuffers, SwapchainImageIndex),
    };

    VkSubmitInfo SubmitInfos[1] = {};
    SubmitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfos[0].waitSemaphoreCount = CTK_ARRAY_COUNT(DirectWaitSemaphores);
    SubmitInfos[0].pWaitSemaphores = DirectWaitSemaphores;
    SubmitInfos[0].pWaitDstStageMask = DirectWaitStages;
    SubmitInfos[0].commandBufferCount = CTK_ARRAY_COUNT(DirectCommandBuffers);
    SubmitInfos[0].pCommandBuffers = DirectCommandBuffers;
    SubmitInfos[0].signalSemaphoreCount = CTK_ARRAY_COUNT(DirectSignalSemaphores);
    SubmitInfos[0].pSignalSemaphores = DirectSignalSemaphores;

    vtk::validate_vk_result(vkQueueSubmit(Device->GraphicsQueue, CTK_ARRAY_COUNT(SubmitInfos), SubmitInfos, CurrentFrame->InFlightFence),
                            "vkQueueSubmit", "failed to submit command buffer to graphics queue");

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

    // Cycle frame.
    FrameState->CurrentFrameIndex = (FrameState->CurrentFrameIndex + 1) % FrameState->Frames.Count;
}
