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
                Vertex->UV = { UV->x, 1 - UV->y }; // Blender's UV y-axis is inverse from Vulkan's.
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

static void
create_deferred_rendering_state(vulkan_instance *VulkanInstance, assets *Assets)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;

    ////////////////////////////////////////////////////////////
    /// Render Target Textures
    ////////////////////////////////////////////////////////////
    VkSamplerCreateInfo RenderTargetSamplerCreateInfo = {};
    RenderTargetSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    RenderTargetSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    RenderTargetSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    RenderTargetSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    RenderTargetSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    RenderTargetSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    RenderTargetSamplerCreateInfo.anisotropyEnable = VK_TRUE;
    RenderTargetSamplerCreateInfo.maxAnisotropy = 16;
    RenderTargetSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    RenderTargetSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    RenderTargetSamplerCreateInfo.compareEnable = VK_FALSE;
    RenderTargetSamplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    RenderTargetSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    RenderTargetSamplerCreateInfo.mipLodBias = 0.0f;
    RenderTargetSamplerCreateInfo.minLod = 0.0f;
    RenderTargetSamplerCreateInfo.maxLod = 0.0f;

    // Albedo Texture
    vtk::texture *AlbedoTexture = ctk::Push(&Assets->Textures, "deferred_albedo");

    vtk::image_info AlbedoImageInfo = {};
    AlbedoImageInfo.Width = Swapchain->Extent.width;
    AlbedoImageInfo.Height = Swapchain->Extent.height;
    AlbedoImageInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
    AlbedoImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    AlbedoImageInfo.UsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    AlbedoImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    AlbedoImageInfo.AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    AlbedoTexture->Image = vtk::CreateImage(Device, &AlbedoImageInfo);
    AlbedoTexture->Image.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; ctk::Todo("hack image layout to work with descriptor set updating");

    vtk::ValidateVkResult(vkCreateSampler(Device->Logical, &RenderTargetSamplerCreateInfo, NULL, &AlbedoTexture->Sampler),
                          "vkCreateSampler", "failed to create texture sampler");

    // Position Texture
    vtk::texture *PositionTexture = ctk::Push(&Assets->Textures, "deferred_position");

    vtk::image_info PositionImageInfo = {};
    PositionImageInfo.Width = Swapchain->Extent.width;
    PositionImageInfo.Height = Swapchain->Extent.height;
    PositionImageInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
    PositionImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    PositionImageInfo.UsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    PositionImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    PositionImageInfo.AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    PositionTexture->Image = vtk::CreateImage(Device, &PositionImageInfo);
    PositionTexture->Image.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; ctk::Todo("hack image layout to work with descriptor set updating");

    vtk::ValidateVkResult(vkCreateSampler(Device->Logical, &RenderTargetSamplerCreateInfo, NULL, &PositionTexture->Sampler),
                          "vkCreateSampler", "failed to create texture sampler");
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

    // ////////////////////////////////////////////////////////////
    // /// Direct Render Pass
    // ////////////////////////////////////////////////////////////
    // vtk::render_pass *DirectRenderPass = ctk::Push(&VulkanInstance->RenderPasses, "direct");

    // // Render Pass Info
    // vtk::render_pass_info DirectRenderPassInfo = {};

    // // Attachments
    // u32 DirectColorAttachmentIndex = DirectRenderPassInfo.Attachments.Count;
    // vtk::attachment *DirectColorAttachment = ctk::Push(&DirectRenderPassInfo.Attachments);
    // DirectColorAttachment->Description.format = Swapchain->ImageFormat;
    // DirectColorAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    // DirectColorAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // DirectColorAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // DirectColorAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    // DirectColorAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // DirectColorAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // DirectColorAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // DirectColorAttachment->ClearValue = COLOR_ATTACHMENT_CLEAR_VALUE;

    // u32 DirectDepthAttachmentIndex = DirectRenderPassInfo.Attachments.Count;
    // ctk::Push(&DirectRenderPassInfo.Attachments, DepthAttachment);

    // // Subpasses
    // vtk::subpass *DirectSubpass = ctk::Push(&DirectRenderPassInfo.Subpasses);

    // VkAttachmentReference *DirectColorAttachmentReference = ctk::Push(&DirectSubpass->ColorAttachmentReferences);
    // DirectColorAttachmentReference->attachment = DirectColorAttachmentIndex;
    // DirectColorAttachmentReference->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // DirectSubpass->DepthAttachmentReference.Set = true;
    // DirectSubpass->DepthAttachmentReference.Value.attachment = DirectDepthAttachmentIndex;
    // DirectSubpass->DepthAttachmentReference.Value.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // // Framebuffer Infos
    // for(u32 FramebufferIndex = 0; FramebufferIndex < Swapchain->Images.Count; ++FramebufferIndex)
    // {
    //     vtk::framebuffer_info *FramebufferInfo = ctk::Push(&DirectRenderPassInfo.FramebufferInfos);
    //     ctk::Push(&FramebufferInfo->Attachments, Swapchain->Images[FramebufferIndex].View);
    //     ctk::Push(&FramebufferInfo->Attachments, DepthImage->View);
    //     FramebufferInfo->Extent = Swapchain->Extent;
    //     FramebufferInfo->Layers = 1;
    // }

    // *DirectRenderPass = vtk::CreateRenderPass(Device->Logical, VulkanInstance->GraphicsCommandPool, &DirectRenderPassInfo);

    ////////////////////////////////////////////////////////////
    /// Deferred Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass *DeferredRenderPass = ctk::Push(&VulkanState->RenderPasses, "deferred");
    vtk::image *AlbedoImage = &ctk::At(&Assets->Textures, "deferred_albedo")->Image;
    vtk::image *PositionImage = &ctk::At(&Assets->Textures, "deferred_position")->Image;

    // Render Pass Info
    vtk::render_pass_info DeferredRenderPassInfo = {};

    // Attachments
    u32 DeferredAlbedoAttachmentIndex = DeferredRenderPassInfo.Attachments.Count;
    vtk::attachment *DeferredAlbedoAttachment = ctk::Push(&DeferredRenderPassInfo.Attachments);
    DeferredAlbedoAttachment->Description.format = AlbedoImage->Format;
    DeferredAlbedoAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DeferredAlbedoAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DeferredAlbedoAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    DeferredAlbedoAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    DeferredAlbedoAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DeferredAlbedoAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DeferredAlbedoAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    DeferredAlbedoAttachment->ClearValue = COLOR_ATTACHMENT_CLEAR_VALUE;

    u32 DeferredPositionAttachmentIndex = DeferredRenderPassInfo.Attachments.Count;
    vtk::attachment *DeferredPositionAttachment = ctk::Push(&DeferredRenderPassInfo.Attachments);
    DeferredPositionAttachment->Description.format = PositionImage->Format;
    DeferredPositionAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DeferredPositionAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DeferredPositionAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    DeferredPositionAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    DeferredPositionAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    DeferredPositionAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DeferredPositionAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    DeferredPositionAttachment->ClearValue = COLOR_ATTACHMENT_CLEAR_VALUE;

    u32 DeferredDepthAttachmentIndex = DeferredRenderPassInfo.Attachments.Count;
    ctk::Push(&DeferredRenderPassInfo.Attachments, DepthAttachment);

    // Subpasses
    vtk::subpass *DeferredSubpass = ctk::Push(&DeferredRenderPassInfo.Subpasses);

    VkAttachmentReference *DeferredAlbedoAttachmentReference = ctk::Push(&DeferredSubpass->ColorAttachmentReferences);
    DeferredAlbedoAttachmentReference->attachment = DeferredAlbedoAttachmentIndex;
    DeferredAlbedoAttachmentReference->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference *DeferredPositionAttachmentReference = ctk::Push(&DeferredSubpass->ColorAttachmentReferences);
    DeferredPositionAttachmentReference->attachment = DeferredPositionAttachmentIndex;
    DeferredPositionAttachmentReference->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    DeferredSubpass->DepthAttachmentReference.Set = true;
    DeferredSubpass->DepthAttachmentReference.Value.attachment = DeferredDepthAttachmentIndex;
    DeferredSubpass->DepthAttachmentReference.Value.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Framebuffer Infos
    vtk::framebuffer_info *FramebufferInfo = ctk::Push(&DeferredRenderPassInfo.FramebufferInfos);
    ctk::Push(&FramebufferInfo->Attachments, AlbedoImage->View);
    ctk::Push(&FramebufferInfo->Attachments, PositionImage->View);
    ctk::Push(&FramebufferInfo->Attachments, DepthImage->View);
    FramebufferInfo->Extent = Swapchain->Extent;
    FramebufferInfo->Layers = 1;

    *DeferredRenderPass = vtk::CreateRenderPass(Device->Logical, VulkanInstance->GraphicsCommandPool, &DeferredRenderPassInfo);

    ////////////////////////////////////////////////////////////
    /// Lighting Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass *LightingRenderPass = ctk::Push(&VulkanState->RenderPasses, "lighting");

    // Render Pass Info
    vtk::render_pass_info LightingRenderPassInfo = {};

    // Attachments
    u32 LightingColorAttachmentIndex = LightingRenderPassInfo.Attachments.Count;
    vtk::attachment *LightingColorAttachment = ctk::Push(&LightingRenderPassInfo.Attachments);
    LightingColorAttachment->Description.format = Swapchain->ImageFormat;
    LightingColorAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    LightingColorAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    LightingColorAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    LightingColorAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    LightingColorAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    LightingColorAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    LightingColorAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    LightingColorAttachment->ClearValue = COLOR_ATTACHMENT_CLEAR_VALUE;

    u32 LightingDepthAttachmentIndex = LightingRenderPassInfo.Attachments.Count;
    ctk::Push(&LightingRenderPassInfo.Attachments, DepthAttachment);

    // Subpasses
    vtk::subpass *LightingSubpass = ctk::Push(&LightingRenderPassInfo.Subpasses);

    VkAttachmentReference *LightingColorAttachmentReference = ctk::Push(&LightingSubpass->ColorAttachmentReferences);
    LightingColorAttachmentReference->attachment = LightingColorAttachmentIndex;
    LightingColorAttachmentReference->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    LightingSubpass->DepthAttachmentReference.Set = true;
    LightingSubpass->DepthAttachmentReference.Value.attachment = LightingDepthAttachmentIndex;
    LightingSubpass->DepthAttachmentReference.Value.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Framebuffer Infos
    for(u32 FramebufferIndex = 0; FramebufferIndex < Swapchain->Images.Count; ++FramebufferIndex)
    {
        vtk::framebuffer_info *FramebufferInfo = ctk::Push(&LightingRenderPassInfo.FramebufferInfos);
        ctk::Push(&FramebufferInfo->Attachments, Swapchain->Images[FramebufferIndex].View);
        ctk::Push(&FramebufferInfo->Attachments, DepthImage->View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    *LightingRenderPass = vtk::CreateRenderPass(Device->Logical, VulkanInstance->GraphicsCommandPool, &LightingRenderPassInfo);
}

////////////////////////////////////////////////////////////
/// Interface
////////////////////////////////////////////////////////////
window *
create_window(input_state *InputState)
{
    auto Window = ctk::Alloc<window>();
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
    auto VulkanInstance = ctk::Alloc<vulkan_instance>();
    *VulkanInstance = {};

    vtk::instance *Instance = &VulkanInstance->Instance;
    VkSurfaceKHR *PlatformSurface = &VulkanInstance->PlatformSurface;
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    VkCommandPool *GraphicsCommandPool = &VulkanInstance->GraphicsCommandPool;
    vtk::buffer *HostBuffer = &VulkanInstance->HostBuffer;

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

    // Swapchain
    *Swapchain = vtk::CreateSwapchain(Device, *PlatformSurface);

    // Graphics Command Pool
    *GraphicsCommandPool = vtk::CreateCommandPool(Device->Logical, Device->QueueFamilyIndexes.Graphics);

    // Frame State
    VulkanInstance->FrameState = vtk::CreateFrameState(Device->Logical, 2, Swapchain->Images.Count);

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
    auto Assets = ctk::Alloc<assets>();
    *Assets = {};

    vtk::device *Device = &VulkanInstance->Device;

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

    ////////////////////////////////////////////////////////////
    /// Deferred Rendering State
    ////////////////////////////////////////////////////////////
    create_deferred_rendering_state(VulkanInstance, Assets);

    return Assets;
}

vulkan_state *
create_vulkan_state(vulkan_instance *VulkanInstance, assets *Assets)
{
    auto VulkanState = ctk::Alloc<vulkan_state>();
    *VulkanState = {};

    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;

    VkDescriptorPool *DescriptorPool = &VulkanState->DescriptorPool;
    vtk::vertex_layout *VertexLayout = &VulkanState->VertexLayout;
    auto *UniformBuffers = &VulkanState->UniformBuffers;
    auto *DescriptorSets = &VulkanState->DescriptorSets;
    auto *VertexAttributeIndexes = &VulkanState->VertexAttributeIndexes;

    ctk::data VulkanStateData = ctk::LoadData("assets/data/vulkan_state.ctkd");

    ////////////////////////////////////////////////////////////
    /// Predefined State
    ////////////////////////////////////////////////////////////
    ctk::Todo("using frame count instead of swapchain image count");

    // Uniform Buffers
    ctk::Push(UniformBuffers, "entity",
              vtk::CreateUniformBuffer(&VulkanInstance->HostBuffer, 64, sizeof(entity_ubo), FrameState->Frames.Count));
    ctk::Push(UniformBuffers, "light",
              vtk::CreateUniformBuffer(&VulkanInstance->HostBuffer, 64, sizeof(light_ubo), FrameState->Frames.Count));

    // Depth Image
    vtk::image_info DepthImageInfo = {};
    DepthImageInfo.Width = Swapchain->Extent.width;
    DepthImageInfo.Height = Swapchain->Extent.height;
    DepthImageInfo.Format = vtk::FindDepthImageFormat(Device->Physical);
    DepthImageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
    DepthImageInfo.UsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    DepthImageInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    DepthImageInfo.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    VulkanState->DepthImage = vtk::CreateImage(Device, &DepthImageInfo);

    // Render Passes
    create_render_passes(VulkanInstance, Assets, VulkanState);

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
        for(u32 DescriptorSetLayoutIndex = 0; DescriptorSetLayoutIndex < DescriptorSetLayoutArray->Children.Count;
            ++DescriptorSetLayoutIndex)
        {
            ctk::Push(&GraphicsPipelineInfo.DescriptorSetLayouts,
                      ctk::At(DescriptorSets, ctk::CStr(DescriptorSetLayoutArray, DescriptorSetLayoutIndex))->Layout);
        }
        for(u32 VertexInputIndex = 0; VertexInputIndex < VertexInputArray->Children.Count; ++VertexInputIndex)
        {
            ctk::data *VertexInputData = ctk::At(VertexInputArray, VertexInputIndex);
            ctk::Push(&GraphicsPipelineInfo.VertexInputs,
                      {
                          ctk::U32(VertexInputData, "binding"),
                          ctk::U32(VertexInputData, "location"),
                          *ctk::At(VertexAttributeIndexes, ctk::CStr(VertexInputData, "attribute"))
                      });
        }
        GraphicsPipelineInfo.VertexLayout = VertexLayout;
        GraphicsPipelineInfo.ViewportExtent = Swapchain->Extent;
        GraphicsPipelineInfo.PrimitiveTopology = vtk::GetVkPrimitiveTopology(ctk::CStr(GraphicsPipelineData, "primitive_topology"));
        GraphicsPipelineInfo.DepthTesting = vtk::GetVkBool32(ctk::CStr(GraphicsPipelineData, "depth_testing"));
        vtk::render_pass *RenderPass = At(&VulkanState->RenderPasses, ctk::CStr(GraphicsPipelineData, "render_pass"));
        ctk::Push(&VulkanState->GraphicsPipelines, GraphicsPipelineData->Key.Data,
                  vtk::CreateGraphicsPipeline(Device->Logical, RenderPass, &GraphicsPipelineInfo));
    }

    VulkanState->DeferredRenderingFinishedSemaphore = vtk::CreateSemaphore(Device->Logical);

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
    vtk::WriteToHostRegion(VulkanInstance->Device.Logical, EntityUniformBufferRegion,
                           Scene->EntityUBOs.Data, ctk::ByteCount(&Scene->EntityUBOs), 0);
}

// void
// record_direct_render_pass(vulkan_instance *VulkanInstance, scene *Scene)
// {
//     vtk::device *Device = &VulkanInstance->Device;
//     vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
//     vtk::frame_state *FrameState = &VulkanInstance->FrameState;
//     vtk::render_pass *DirectRenderPass = ctk::At(&VulkanInstance->RenderPasses, "direct");

//     VkRect2D RenderArea = {};
//     RenderArea.offset.x = 0;
//     RenderArea.offset.y = 0;
//     RenderArea.extent = Swapchain->Extent;

//     VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
//     CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//     CommandBufferBeginInfo.flags = 0;
//     CommandBufferBeginInfo.pInheritanceInfo = NULL;

//     ctk::Todo("using frame count instead of swapchain image count");
//     for(u32 FrameIndex = 0; FrameIndex < FrameState->Frames.Count; ++FrameIndex)
//     {
//         VkCommandBuffer CommandBuffer = *At(&DirectRenderPass->CommandBuffers, FrameIndex);
//         vtk::ValidateVkResult(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
//                               "vkBeginCommandBuffer", "failed to begin recording command buffer");
//         VkRenderPassBeginInfo RenderPassBeginInfo = {};
//         RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
//         RenderPassBeginInfo.renderPass = DirectRenderPass->Handle;
//         RenderPassBeginInfo.framebuffer = *At(&DirectRenderPass->Framebuffers, FrameIndex);
//         RenderPassBeginInfo.renderArea = RenderArea;
//         RenderPassBeginInfo.clearValueCount = DirectRenderPass->ClearValues.Count;
//         RenderPassBeginInfo.pClearValues = DirectRenderPass->ClearValues.Data;

//         // Begin
//         vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

//         ////////////////////////////////////////////////////////////
//         /// Render Commands
//         ////////////////////////////////////////////////////////////
//         for(u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex)
//         {
//             entity *Entity = Scene->Entities.Values + EntityIndex;
//             mesh *Mesh = Entity->Mesh;

//             // Graphics Pipeline
//             vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Entity->GraphicsPipeline->Handle);

//             // Descriptor Sets
//             vtk::BindDescriptorSets(CommandBuffer, Entity->GraphicsPipeline->Layout,
//                                     Entity->DescriptorSets.Data, Entity->DescriptorSets.Count,
//                                     FrameIndex, EntityIndex);

//             // Vertex/Index Buffers
//             vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
//             vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);

//             // Draw
//             vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
//         }

//         // End
//         vkCmdEndRenderPass(CommandBuffer);
//         vtk::ValidateVkResult(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
//     }
// }

void
record_deferred_render_pass(vulkan_instance *VulkanInstance, vulkan_state *VulkanState, scene *Scene)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state *FrameState = &VulkanInstance->FrameState;
    vtk::render_pass *DeferredRenderPass = ctk::At(&VulkanState->RenderPasses, "deferred");

    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = Swapchain->Extent;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

    VkCommandBuffer CommandBuffer = *At(&DeferredRenderPass->CommandBuffers, 0);
    vtk::ValidateVkResult(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                          "vkBeginCommandBuffer", "failed to begin recording command buffer");
    VkRenderPassBeginInfo RenderPassBeginInfo = {};
    RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RenderPassBeginInfo.renderPass = DeferredRenderPass->Handle;
    RenderPassBeginInfo.framebuffer = *At(&DeferredRenderPass->Framebuffers, 0);
    RenderPassBeginInfo.renderArea = RenderArea;
    RenderPassBeginInfo.clearValueCount = DeferredRenderPass->ClearValues.Count;
    RenderPassBeginInfo.pClearValues = DeferredRenderPass->ClearValues.Data;

    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        for(u32 EntityIndex = 0; EntityIndex < Scene->Entities.Count; ++EntityIndex)
        {
            entity *Entity = Scene->Entities.Values + EntityIndex;
            mesh *Mesh = Entity->Mesh;

            // Graphics Pipeline
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Entity->GraphicsPipeline->Handle);

            // Descriptor Sets
            vtk::BindDescriptorSets(CommandBuffer, Entity->GraphicsPipeline->Layout,
                                    Entity->DescriptorSets.Data, Entity->DescriptorSets.Count,
                                    0, EntityIndex);

            // Vertex/Index Buffers
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
            vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);

            // Draw
            vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
        }
    vkCmdEndRenderPass(CommandBuffer);
    vtk::ValidateVkResult(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
}

void
record_lighting_render_pass(vulkan_instance *VulkanInstance, vulkan_state *VulkanState, assets *Assets)
{
    vtk::device *Device = &VulkanInstance->Device;
    vtk::swapchain *Swapchain = &VulkanInstance->Swapchain;
    vtk::render_pass *LightingRenderPass = ctk::At(&VulkanState->RenderPasses, "lighting");

    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = Swapchain->Extent;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = 0;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

    mesh *Mesh = ctk::At(&Assets->Meshes, "fullscreen_plane");
    vtk::graphics_pipeline *GraphicsPipeline = ctk::At(&VulkanState->GraphicsPipelines, "lighting");
    vtk::descriptor_set *DescriptorSet = ctk::At(&VulkanState->DescriptorSets, "deferred_textures");

    for(u32 SwapchainImageIndex = 0; SwapchainImageIndex < Swapchain->Images.Count; ++SwapchainImageIndex)
    {
        VkCommandBuffer CommandBuffer = *At(&LightingRenderPass->CommandBuffers, SwapchainImageIndex);
        vtk::ValidateVkResult(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                              "vkBeginCommandBuffer", "failed to begin recording command buffer");
        VkRenderPassBeginInfo RenderPassBeginInfo = {};
        RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassBeginInfo.renderPass = LightingRenderPass->Handle;
        RenderPassBeginInfo.framebuffer = *At(&LightingRenderPass->Framebuffers, SwapchainImageIndex);
        RenderPassBeginInfo.renderArea = RenderArea;
        RenderPassBeginInfo.clearValueCount = LightingRenderPass->ClearValues.Count;
        RenderPassBeginInfo.pClearValues = LightingRenderPass->ClearValues.Data;

        vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            // Graphics Pipeline
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline->Handle);

            // Descriptor Sets
            vtk::BindDescriptorSets(CommandBuffer, GraphicsPipeline->Layout, &DescriptorSet, 1, 0, 0);

            // Vertex/Index Buffers
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh->VertexRegion.Buffer->Handle, &Mesh->VertexRegion.Offset);
            vkCmdBindIndexBuffer(CommandBuffer, Mesh->IndexRegion.Buffer->Handle, Mesh->IndexRegion.Offset, VK_INDEX_TYPE_UINT32);

            // Draw
            vkCmdDrawIndexed(CommandBuffer, Mesh->Indexes.Count, 1, 0, 0, 0);
        vkCmdEndRenderPass(CommandBuffer);
        vtk::ValidateVkResult(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during render pass command recording");
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
    /// Command Buffers Submission
    ////////////////////////////////////////////////////////////

    // Submit Deferred Command Buffer
    VkSemaphore DeferredWaitSemaphores[] = { CurrentFrame->ImageAquiredSemaphore };
    VkPipelineStageFlags DeferredWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore DeferredSignalSemaphores[] = { VulkanState->DeferredRenderingFinishedSemaphore };
    VkCommandBuffer DeferredCommandBuffers[] =
    {
        *ctk::At(&ctk::At(&VulkanState->RenderPasses, "deferred")->CommandBuffers, 0),
    };

    // Submit Lighting Command Buffer
    VkSemaphore LightingWaitSemaphores[] = { VulkanState->DeferredRenderingFinishedSemaphore };
    VkPipelineStageFlags LightingWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore LightingSignalSemaphores[] = { CurrentFrame->RenderFinishedSemaphore };
    VkCommandBuffer LightingCommandBuffers[] =
    {
        *ctk::At(&ctk::At(&VulkanState->RenderPasses, "lighting")->CommandBuffers, SwapchainImageIndex),
    };

    VkSubmitInfo SubmitInfos[2] = {};

    // Deferred
    SubmitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfos[0].waitSemaphoreCount = CTK_ARRAY_COUNT(DeferredWaitSemaphores);
    SubmitInfos[0].pWaitSemaphores = DeferredWaitSemaphores;
    SubmitInfos[0].pWaitDstStageMask = DeferredWaitStages;
    SubmitInfos[0].commandBufferCount = CTK_ARRAY_COUNT(DeferredCommandBuffers);
    SubmitInfos[0].pCommandBuffers = DeferredCommandBuffers;
    SubmitInfos[0].signalSemaphoreCount = CTK_ARRAY_COUNT(DeferredSignalSemaphores);
    SubmitInfos[0].pSignalSemaphores = DeferredSignalSemaphores;

    // Lighting
    SubmitInfos[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfos[1].waitSemaphoreCount = CTK_ARRAY_COUNT(LightingWaitSemaphores);
    SubmitInfos[1].pWaitSemaphores = LightingWaitSemaphores;
    SubmitInfos[1].pWaitDstStageMask = LightingWaitStages;
    SubmitInfos[1].commandBufferCount = CTK_ARRAY_COUNT(LightingCommandBuffers);
    SubmitInfos[1].pCommandBuffers = LightingCommandBuffers;
    SubmitInfos[1].signalSemaphoreCount = CTK_ARRAY_COUNT(LightingSignalSemaphores);
    SubmitInfos[1].pSignalSemaphores = LightingSignalSemaphores;

    vtk::ValidateVkResult(vkQueueSubmit(Device->GraphicsQueue, CTK_ARRAY_COUNT(SubmitInfos), SubmitInfos, CurrentFrame->InFlightFence),
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
    vtk::ValidateVkResult(vkQueuePresentKHR(Device->PresentQueue, &PresentInfo), "vkQueuePresentKHR",
                          "failed to queue image for presentation");

    // Cycle frame.
    FrameState->CurrentFrameIndex = (FrameState->CurrentFrameIndex + 1) % FrameState->Frames.Count;
}
