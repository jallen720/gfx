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
    ctk::Todo("HACK: Baking all meshes from file into single mesh.");
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
    Mesh.Vertexes = ctk::CreateArrayEmpty<vertex>(VertexCount);
    Mesh.Indexes = ctk::CreateArrayEmpty<u32>(IndexCount);

    ////////////////////////////////////////////////////////////
    /// Processing
    ////////////////////////////////////////////////////////////
    for(u32 MeshIndex = 0; MeshIndex < Scene->mNumMeshes; ++MeshIndex)
    {
        aiMesh *SceneMesh = Scene->mMeshes[MeshIndex];
        u32 IndexBase = Mesh.Vertexes.Count;
        for(u32 VertexIndex = 0; VertexIndex < SceneMesh->mNumVertices; ++VertexIndex)
        {
            vertex *Vertex = ctk::Push(&Mesh.Vertexes);
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
                Vertex->UV = { UV->x, -UV->y }; // Blender's UV y-axis is inverse from Vulkan's.
            }
        }
        for(u32 FaceIndex = 0; FaceIndex < SceneMesh->mNumFaces; ++FaceIndex)
        {
            aiFace *Face = SceneMesh->mFaces + FaceIndex;
            for(u32 IndexIndex = 0; IndexIndex < Face->mNumIndices; ++IndexIndex)
            {
                ctk::Push(&Mesh.Indexes, IndexBase + Face->mIndices[IndexIndex]);
            }
        }
    }

    // Allocate and write vertex/index data to their associated regions.
    u32 VertexByteSize = ctk::ByteSize(&Mesh.Vertexes);
    u32 IndexByteSize = ctk::ByteSize(&Mesh.Indexes);
    Mesh.VertexRegion = vtk::AllocateRegion(DeviceBuffer, VertexByteSize);
    Mesh.IndexRegion = vtk::AllocateRegion(DeviceBuffer, IndexByteSize);
    vtk::WriteToDeviceRegion(Device, GraphicsCommandPool, StagingRegion, &Mesh.VertexRegion,
                             Mesh.Vertexes.Data, VertexByteSize, 0);
    vtk::WriteToDeviceRegion(Device, GraphicsCommandPool, StagingRegion, &Mesh.IndexRegion,
                             Mesh.Indexes.Data, IndexByteSize, 0);

    // Cleanup
    aiReleaseImport(Scene);

    return Mesh;
}

////////////////////////////////////////////////////////////
/// Interface
////////////////////////////////////////////////////////////
window *
create_window(input_state *InputState)
{
    window *Window = ctk::Alloc<window>();
    *Window = {};
    ctk::data Data = ctk::LoadData("assets/data/window.ctkd");
    glfwSetErrorCallback(error_callback);
    if(glfwInit() != GLFW_TRUE)
    {
        CTK_FATAL("failed to init GLFW")
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    Window->Width = S32(&Data, "width");
    Window->Height = S32(&Data, "height");
    Window->Handle = glfwCreateWindow(Window->Width, Window->Height, CStr(&Data, "title"), NULL, NULL);
    if(Window->Handle == NULL)
    {
        CTK_FATAL("failed to create window")
    }
    glfwSetWindowPos(Window->Handle, S32(&Data, "x"), S32(&Data, "y"));
    glfwSetWindowUserPointer(Window->Handle, (void *)InputState);
    // glfwSetFramebufferSizeCallback(Window->Handle, FramebufferResizeCallback);
    glfwSetKeyCallback(Window->Handle, key_callback);
    glfwSetMouseButtonCallback(Window->Handle, mouse_button_callback);
    return Window;
}

vulkan_instance *
create_vulkan_instance(window *Window)
{
    vulkan_instance *VulkanInstance = ctk::Alloc<vulkan_instance>();
    *VulkanInstance = {};

    vtk::instance *Instance = &VulkanInstance->Instance;
    VkSurfaceKHR *PlatformSurface = &VulkanInstance->PlatformSurface;
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    VkCommandPool *GraphicsCommandPool = &VulkanInstance->GraphicsCommandPool;
    vtk::image *DepthImage = &VulkanInstance->DepthImage;
    vtk::render_pass *RenderPass = &VulkanInstance->RenderPass;
    vtk::buffer *HostBuffer = &VulkanInstance->HostBuffer;

    // Load data file.
    ctk::data VulkanInstanceData = ctk::LoadData("assets/data/vulkan_instance.ctkd");

    // Instance
    u32 GLFWExtensionCount = 0;
    cstr *GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionCount);
    vtk::instance_info InstanceInfo = {};
    ctk::Push(&InstanceInfo.Extensions, GLFWExtensions, GLFWExtensionCount);
    InstanceInfo.Debug = ctk::B32(&VulkanInstanceData, "debug");
    InstanceInfo.AppName = ctk::CStr(&VulkanInstanceData, "app_name");
    *Instance = vtk::CreateInstance(&InstanceInfo);

    // Platform Surface
    vtk::ValidateVkResult(glfwCreateWindowSurface(Instance->Handle, Window->Handle, NULL, PlatformSurface),
                          "glfwCreateWindowSurface", "failed to create GLFW surface");

    // Device
    vtk::device_info DeviceInfo = {};
    ctk::Push(&DeviceInfo.Extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME); // Swapchains required for rendering.
    DeviceInfo.Features.geometryShader = VK_TRUE;
    DeviceInfo.Features.samplerAnisotropy = VK_TRUE;
    // DeviceInfo.Features.vertexPipelineStoresAndAtomics = VK_TRUE;
    *Device = vtk::CreateDevice(Instance->Handle, *PlatformSurface, &DeviceInfo);

    // Graphics Command Pool
    *GraphicsCommandPool = vtk::CreateCommandPool(Device->Logical, Device->QueueFamilyIndexes.Graphics);

    // Swapchain
    *Swapchain = vtk::CreateSwapchain(Device, *PlatformSurface);
    for(u32 i = 0; i < Swapchain->Images.Count; ++i)
    {
        TransitionImageLayout(Device, *GraphicsCommandPool, Swapchain->Images + i,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    // Frame State
    VulkanInstance->FrameState = vtk::CreateFrameState(Device->Logical, 2, Swapchain->Images.Count);

    // Depth Image
    vtk::image_info DepthImageInfo = {};
    DepthImageInfo.Width = Swapchain->Extent.width;
    DepthImageInfo.Height = Swapchain->Extent.height;
    DepthImageInfo.Format = vtk::FindDepthImageFormat(Device->Physical);
    DepthImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    DepthImageInfo.UsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    DepthImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    DepthImageInfo.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    *DepthImage = vtk::CreateImage(Device, &DepthImageInfo);

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass_info RenderPassInfo = {};

    // Attachments
    u32 ColorAttachmentIndex = RenderPassInfo.Attachments.Count;
    vtk::attachment *ColorAttachment = ctk::Push(&RenderPassInfo.Attachments);
    ColorAttachment->Description.format = Swapchain->ImageFormat;
    ColorAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ColorAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    ColorAttachment->ClearValue = { 0.04f, 0.04f, 0.04f, 1.0f };

    u32 DepthAttachmentIndex = RenderPassInfo.Attachments.Count;
    vtk::attachment *DepthAttachment = ctk::Push(&RenderPassInfo.Attachments);
    DepthAttachment->Description.format = DepthImage->Format;
    DepthAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    DepthAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DepthAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    DepthAttachment->ClearValue = { 1.0f, 0 };

    // Subpasses
    vtk::subpass *Subpass = ctk::Push(&RenderPassInfo.Subpasses);

    VkAttachmentReference *ColorAttachmentReference = ctk::Push(&Subpass->ColorAttachmentReferences);
    ColorAttachmentReference->attachment = ColorAttachmentIndex;
    ColorAttachmentReference->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    Subpass->DepthAttachmentReference.Set = true;
    Subpass->DepthAttachmentReference.Value.attachment = DepthAttachmentIndex;
    Subpass->DepthAttachmentReference.Value.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Framebuffer Infos
    for(u32 FramebufferIndex = 0; FramebufferIndex < Swapchain->Images.Count; ++FramebufferIndex)
    {
        vtk::framebuffer_info *FramebufferInfo = ctk::Push(&RenderPassInfo.FramebufferInfos);
        // ctk::Push(&FramebufferInfo->Attachments, VulkanInstance->RenderPassImages[FramebufferIndex].View);
        ctk::Push(&FramebufferInfo->Attachments, Swapchain->Images[FramebufferIndex].View);
        ctk::Push(&FramebufferInfo->Attachments, DepthImage->View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    // Creation
    *RenderPass = vtk::CreateRenderPass(Device->Logical, *GraphicsCommandPool, &RenderPassInfo);

    ////////////////////////////////////////////////////////////
    /// Memory
    ////////////////////////////////////////////////////////////

    // Buffers
    vtk::buffer_info HostBufferInfo = {};
    HostBufferInfo.Size = 100 * CTK_MEGABYTE;
    HostBufferInfo.UsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    HostBufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    *HostBuffer = vtk::CreateBuffer(Device, &HostBufferInfo);

    vtk::buffer_info DeviceBufferInfo = {};
    DeviceBufferInfo.Size = 100 * CTK_MEGABYTE;
    DeviceBufferInfo.UsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    DeviceBufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VulkanInstance->DeviceBuffer = vtk::CreateBuffer(Device, &DeviceBufferInfo);

    // Regions
    VulkanInstance->StagingRegion = vtk::AllocateRegion(HostBuffer, 50 * CTK_MEGABYTE);

    return VulkanInstance;
}

assets *
create_assets(vulkan_instance *VulkanInstance)
{
    assets *Assets = ctk::Alloc<assets>();
    *Assets = {};

    vtk::device *Device = &VulkanInstance->Device;

    // Load data file.
    ctk::data AssetData = ctk::LoadData("assets/data/assets.ctkd");

    ////////////////////////////////////////////////////////////
    /// Textures
    ////////////////////////////////////////////////////////////
    ctk::data *TextureMap = ctk::At(&AssetData, "textures");
    for(u32 TextureIndex = 0; TextureIndex < TextureMap->Children.Count; ++TextureIndex)
    {
        ctk::data *TextureData = ctk::At(TextureMap, TextureIndex);
        vtk::texture_info TextureInfo = {};
        TextureInfo.Filter = vtk::GetVkFilter(ctk::CStr(TextureData, "filter"));
        ctk::Push(&Assets->Textures, TextureData->Key.Data,
                  vtk::CreateTexture(Device, VulkanInstance->GraphicsCommandPool, &VulkanInstance->StagingRegion,
                                     ctk::CStr(TextureData, "path"), &TextureInfo));
    }

    ////////////////////////////////////////////////////////////
    /// Shader Modules
    ////////////////////////////////////////////////////////////
    ctk::data *ShaderModuleMap = ctk::At(&AssetData, "shader_modules");
    for(u32 ShaderModuleIndex = 0; ShaderModuleIndex < ShaderModuleMap->Children.Count; ++ShaderModuleIndex)
    {
        ctk::data *ShaderModuleData = ctk::At(ShaderModuleMap, ShaderModuleIndex);
        VkShaderStageFlagBits Stage = vtk::GetVkShaderStageFlagBits(ctk::CStr(ShaderModuleData, "stage"));
        ctk::Push(&Assets->ShaderModules, ShaderModuleData->Key.Data,
                  vtk::CreateShaderModule(Device->Logical, ctk::CStr(ShaderModuleData, "path"), Stage));
    }

    ////////////////////////////////////////////////////////////
    /// Meshes
    ////////////////////////////////////////////////////////////
    ctk::data *ModelMap = ctk::At(&AssetData, "models");
    for(u32 ModelIndex = 0; ModelIndex < ModelMap->Children.Count; ++ModelIndex)
    {
        ctk::data *ModelData = ctk::At(ModelMap, ModelIndex);
        ctk::Push(&Assets->Meshes, ModelData->Key.Data, create_mesh(VulkanInstance, ctk::CStr(ModelData, "path")));
    }

    return Assets;
}

vulkan_state *
create_vulkan_state(vulkan_instance *VulkanInstance, assets *Assets)
{
    vulkan_state *VulkanState = ctk::Alloc<vulkan_state>();
    *VulkanState = {};

    vtk::device *Device = &VulkanInstance->Device;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;

    VkDescriptorPool *DescriptorPool = &VulkanState->DescriptorPool;
    vtk::vertex_layout *VertexLayout = &VulkanState->VertexLayout;
    auto *UniformBuffers = &VulkanState->UniformBuffers;
    auto *DescriptorSets = &VulkanState->DescriptorSets;
    auto *VertexAttributeIndexes = &VulkanState->VertexAttributeIndexes;

    // Load data file.
    ctk::data VulkanStateData = ctk::LoadData("assets/data/vulkan_state.ctkd");

    ////////////////////////////////////////////////////////////
    /// Predefined State
    ////////////////////////////////////////////////////////////
    ctk::Todo("using frame count instead of swapchain image count");
    ctk::Push(UniformBuffers, "entity",
              vtk::CreateUniformBuffer(&VulkanInstance->HostBuffer, 64, sizeof(entity_ubo), FrameState->Frames.Count));
    ctk::Push(UniformBuffers, "light",
              vtk::CreateUniformBuffer(&VulkanInstance->HostBuffer, 64, sizeof(light_ubo), FrameState->Frames.Count));

    ////////////////////////////////////////////////////////////
    /// Descriptor Sets
    ////////////////////////////////////////////////////////////

    // Descriptor Infos
    ctk::smap<vtk::descriptor_info, 8> DescriptorInfos = {};
    ctk::data *DescriptorMap = ctk::At(&VulkanStateData, "descriptors");
    for(u32 DescriptorIndex = 0; DescriptorIndex < DescriptorMap->Children.Count; ++DescriptorIndex)
    {
        ctk::data *DescriptorData = ctk::At(DescriptorMap, DescriptorIndex);
        ctk::data *ShaderStageFlagsArray = ctk::At(DescriptorData, "shader_stage_flags");
        VkDescriptorType Type = vtk::GetVkDescriptorType(ctk::CStr(DescriptorData, "type"));
        VkShaderStageFlags ShaderStageFlags = 0;
        for(u32 ShaderStageIndex = 0; ShaderStageIndex < ShaderStageFlagsArray->Children.Count; ++ShaderStageIndex)
        {
            ShaderStageFlags |= vtk::GetVkShaderStageFlagBits(ctk::CStr(ShaderStageFlagsArray, ShaderStageIndex));
        }

        vtk::descriptor_info* DescriptorInfo = ctk::Push(&DescriptorInfos, DescriptorData->Key.Data);
        DescriptorInfo->Type = Type;
        DescriptorInfo->ShaderStageFlags = ShaderStageFlags;
        DescriptorInfo->Count = ctk::U32(DescriptorData, "count");
        if(Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
           Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        {
            DescriptorInfo->UniformBuffer = ctk::At(UniformBuffers, ctk::CStr(DescriptorData, "uniform_buffer"));
        }
        else if(Type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            DescriptorInfo->Texture = ctk::At(&Assets->Textures, ctk::CStr(DescriptorData, "texture"));
        }
        else
        {
            CTK_FATAL("unhandled descriptor type when loading descriptors")
        }
    }

    // Descriptor Set Infos
    ctk::smap<vtk::descriptor_set_info, 8> DescriptorSetInfos = {};
    ctk::data *DescriptorSetMap = ctk::At(&VulkanStateData, "descriptor_sets");
    for(u32 DescriptorSetIndex = 0; DescriptorSetIndex < DescriptorSetMap->Children.Count; ++DescriptorSetIndex)
    {
        ctk::data *DescriptorSetData = ctk::At(DescriptorSetMap, DescriptorSetIndex);
        vtk::descriptor_set_info *DescriptorSetInfo = ctk::Push(&DescriptorSetInfos, DescriptorSetData->Key.Data);
        b32 IsDynamic = ctk::StringEqual(ctk::CStr(DescriptorSetData, "type"), "dynamic");
        ctk::Todo("using frame count instead of swapchain image count");
        DescriptorSetInfo->InstanceCount = IsDynamic ? FrameState->Frames.Count : 1;
        ctk::data *DescriptorBindingArray = ctk::At(DescriptorSetData, "descriptor_bindings");
        for(u32 DescriptorBindingIndex = 0; DescriptorBindingIndex < DescriptorBindingArray->Children.Count; ++DescriptorBindingIndex)
        {
            ctk::data *DescriptorBindingData = ctk::At(DescriptorBindingArray, DescriptorBindingIndex);
            ctk::Push(&DescriptorSetInfo->DescriptorBindings,
                      {
                          ctk::U32(DescriptorBindingData, "binding"),
                          ctk::At(&DescriptorInfos, ctk::CStr(DescriptorBindingData, "descriptor")),
                      });
        }
    }

    // Pool
    *DescriptorPool = vtk::CreateDescriptorPool(Device->Logical, DescriptorSetInfos.Values, DescriptorSetInfos.Count);

    // Mirror descriptor set infos map to store descriptor sets.
    for(u32 DescriptorSetIndex = 0; DescriptorSetIndex < DescriptorSetInfos.Count; ++DescriptorSetIndex)
    {
        ctk::Push(DescriptorSets, DescriptorSetInfos.Keys[DescriptorSetIndex]);
    }
    vtk::CreateDescriptorSets(Device->Logical, *DescriptorPool, DescriptorSetInfos.Values, DescriptorSetInfos.Count, DescriptorSets->Values);

    ////////////////////////////////////////////////////////////
    /// Vertex Layout
    ////////////////////////////////////////////////////////////
    ctk::data *VertexLayoutMap = ctk::At(&VulkanStateData, "vertex_layout");
    for(u32 VertexAttributeIndex = 0; VertexAttributeIndex < VertexLayoutMap->Children.Count; ++VertexAttributeIndex)
    {
        ctk::data *VertexAttributeData = ctk::At(VertexLayoutMap, VertexAttributeIndex);
        ctk::Push(VertexAttributeIndexes, VertexAttributeData->Key.Data,
                  vtk::PushVertexAttribute(VertexLayout, ctk::U32(VertexAttributeData, "element_count")));
    }

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////
    ctk::data *GraphicsPipelineMap = ctk::At(&VulkanStateData, "graphics_pipelines");
    for(u32 GraphicsPipelineIndex = 0; GraphicsPipelineIndex < GraphicsPipelineMap->Children.Count; ++GraphicsPipelineIndex)
    {
        ctk::data *GraphicsPipelineData = ctk::At(GraphicsPipelineMap, GraphicsPipelineIndex);
        ctk::data *ShaderModuleArray = ctk::At(GraphicsPipelineData, "shader_modules");
        ctk::data *DescriptorSetLayoutArray = ctk::At(GraphicsPipelineData, "descriptor_set_layouts");
        ctk::data *VertexInputArray = ctk::At(GraphicsPipelineData, "vertex_inputs");
        vtk::graphics_pipeline_info GraphicsPipelineInfo = {};
        for(u32 ShaderModuleIndex = 0; ShaderModuleIndex < ShaderModuleArray->Children.Count; ++ShaderModuleIndex)
        {
            ctk::Push(&GraphicsPipelineInfo.ShaderModules,
                      ctk::At(&Assets->ShaderModules, ctk::CStr(ShaderModuleArray, ShaderModuleIndex)));
        }
        for(u32 DescriptorSetLayoutIndex = 0; DescriptorSetLayoutIndex < DescriptorSetLayoutArray->Children.Count; ++DescriptorSetLayoutIndex)
        {
            ctk::Push(&GraphicsPipelineInfo.DescriptorSetLayouts,
                      ctk::At(DescriptorSets, ctk::CStr(DescriptorSetLayoutArray, DescriptorSetLayoutIndex))->Layout);
        }
        for(u32 VertexInputIndex = 0; VertexInputIndex < VertexInputArray->Children.Count; ++VertexInputIndex)
        {
            ctk::data *VertexInputData = ctk::At(VertexInputArray, VertexInputIndex);
            ctk::Push(&GraphicsPipelineInfo.VertexInputs,
                      {
                          ctk::U32(VertexInputData, "location"),
                          ctk::U32(VertexInputData, "binding"),
                          *ctk::At(VertexAttributeIndexes, ctk::CStr(VertexInputData, "attribute"))
                      });
        }
        GraphicsPipelineInfo.VertexLayout = VertexLayout;
        GraphicsPipelineInfo.ViewportExtent = VulkanInstance->Swapchain.Extent;
        GraphicsPipelineInfo.PrimitiveTopology = vtk::GetVkPrimitiveTopology(ctk::CStr(GraphicsPipelineData, "primitive_topology"));
        GraphicsPipelineInfo.DepthTesting = vtk::GetVkBool32(ctk::CStr(GraphicsPipelineData, "depth_testing"));;
        ctk::Push(&VulkanState->GraphicsPipelines, GraphicsPipelineData->Key.Data,
                  vtk::CreateGraphicsPipeline(Device->Logical, VulkanInstance->RenderPass.Handle, &GraphicsPipelineInfo));
    }

    return VulkanState;
}

void
update_uniform_data(scene *Scene, vulkan_instance *VulkanInstance)
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
    vtk::WriteToHostRegion(VulkanInstance->Device.Logical, EntityUniformBufferRegion,
                           Scene->EntityUBOs.Data, ctk::ByteCount(&Scene->EntityUBOs), 0);
}

void
record_render_pass(vulkan_instance *VulkanInstance, scene *Scene)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;
    vtk::render_pass *RenderPass = &VulkanInstance->RenderPass;

    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = Swapchain->Extent;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = 0;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

    ctk::Todo("using frame count instead of swapchain image count");
    for(u32 FrameIndex = 0; FrameIndex < FrameState->Frames.Count; ++FrameIndex)
    {
        VkCommandBuffer CommandBuffer = *At(&RenderPass->CommandBuffers, FrameIndex);
        vtk::ValidateVkResult(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                              "vkBeginCommandBuffer", "failed to begin recording command buffer");
        VkRenderPassBeginInfo RenderPassBeginInfo = {};
        RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassBeginInfo.renderPass = RenderPass->Handle;
        RenderPassBeginInfo.framebuffer = *At(&RenderPass->Framebuffers, FrameIndex);
        RenderPassBeginInfo.renderArea = RenderArea;
        RenderPassBeginInfo.clearValueCount = RenderPass->ClearValues.Count;
        RenderPassBeginInfo.pClearValues = RenderPass->ClearValues.Data;

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
            vtk::BindDescriptorSets(CommandBuffer, Entity->GraphicsPipeline->Layout,
                                    Entity->DescriptorSets.Data, Entity->DescriptorSets.Count,
                                    FrameIndex, EntityIndex);

            // Vertex/Index Buffers
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
            vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);

            // Draw
            vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
        }

        // End
        vkCmdEndRenderPass(CommandBuffer);
        vtk::ValidateVkResult(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
    }
}

void
render(vulkan_instance *VulkanInstance)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;
    vtk::render_pass *RenderPass = &VulkanInstance->RenderPass;

    vtk::frame *CurrentFrame = FrameState->Frames + FrameState->CurrentFrameIndex;

    // Wait on current frame's fence if still unsignaled.
    vkWaitForFences(Device->Logical, 1, &CurrentFrame->InFlightFence, VK_TRUE, UINT64_MAX);

    // Aquire next swapchain image index, using a semaphore to signal when image is available for rendering.
    u32 SwapchainImageIndex = VTK_UNSET_INDEX;
    {
        VkResult Result = vkAcquireNextImageKHR(Device->Logical, Swapchain->Handle, UINT64_MAX, CurrentFrame->ImageAquiredSemaphore,
                                                VK_NULL_HANDLE, &SwapchainImageIndex);
        vtk::ValidateVkResult(Result, "vkAcquireNextImageKHR", "failed to aquire next swapchain image");
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
    /// Submit Render Pass Command Buffer
    ////////////////////////////////////////////////////////////
    VkSemaphore QueueSubmitWaitSemaphores[] = { CurrentFrame->ImageAquiredSemaphore };
    VkPipelineStageFlags QueueSubmitWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore QueueSubmitSignalSemaphores[] = { CurrentFrame->RenderFinishedSemaphore };
    VkCommandBuffer QueueSubmitCommandBuffers[] = { *At(&VulkanInstance->RenderPass.CommandBuffers, SwapchainImageIndex) };

    VkSubmitInfo SubmitInfos[1] = {};
    SubmitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfos[0].waitSemaphoreCount = CTK_ARRAY_COUNT(QueueSubmitWaitSemaphores);
    SubmitInfos[0].pWaitSemaphores = QueueSubmitWaitSemaphores;
    SubmitInfos[0].pWaitDstStageMask = QueueSubmitWaitStages;
    SubmitInfos[0].commandBufferCount = CTK_ARRAY_COUNT(QueueSubmitCommandBuffers);
    SubmitInfos[0].pCommandBuffers = QueueSubmitCommandBuffers;
    SubmitInfos[0].signalSemaphoreCount = CTK_ARRAY_COUNT(QueueSubmitSignalSemaphores);
    SubmitInfos[0].pSignalSemaphores = QueueSubmitSignalSemaphores;

    // Submit render pass commands to graphics queue for rendering.
    // Signal current frame's in flight flence when commands have finished executing.
    vtk::ValidateVkResult(vkQueueSubmit(Device->GraphicsQueue, CTK_ARRAY_COUNT(SubmitInfos), SubmitInfos, CurrentFrame->InFlightFence),
                          "vkQueueSubmit", "failed to submit command buffer to graphics queue");

    ////////////////////////////////////////////////////////////
    /// Presentation
    ////////////////////////////////////////////////////////////

    // Provide 1:1 index per swapchain.
    VkSwapchainKHR Swapchains[] = { Swapchain->Handle };
    u32 SwapchainImageIndexes[] = { SwapchainImageIndex };

    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = CTK_ARRAY_COUNT(QueueSubmitSignalSemaphores);
    PresentInfo.pWaitSemaphores = QueueSubmitSignalSemaphores;
    PresentInfo.swapchainCount = CTK_ARRAY_COUNT(Swapchains);
    PresentInfo.pSwapchains = Swapchains;
    PresentInfo.pImageIndices = SwapchainImageIndexes;
    PresentInfo.pResults = NULL;

    // Submit Swapchains to present queue for presentation once rendering is complete.
    vtk::ValidateVkResult(vkQueuePresentKHR(Device->PresentQueue, &PresentInfo), "vkQueuePresentKHR",
                          "failed to queue image for presentation");

    // Cycle frame.
    FrameState->CurrentFrameIndex = (FrameState->CurrentFrameIndex + 1) % FrameState->Frames.Count;
}
