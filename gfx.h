#pragma once

#include <windows.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "ctk/ctk.h"
#include "ctk/math.h"
#include "ctk/data.h"
#include "vtk/vtk.h"

namespace gfx {

////////////////////////////////////////////////////////////
/// Constants
////////////////////////////////////////////////////////////
static const u32 KILOBYTE = 1000;
static const u32 MEGABYTE = 1000 * KILOBYTE;
static const u32 GIGABYTE = 1000 * MEGABYTE;
static const ctk::vec2<f64> UNSET_MOUSE_POSITION = { -10000.0, -10000.0 };

////////////////////////////////////////////////////////////
/// Data
////////////////////////////////////////////////////////////
struct window
{
    GLFWwindow *Handle;
    u32 Width;
    u32 Height;
};

struct vertex
{
    ctk::vec3<f32> Position;
    ctk::vec3<f32> Normal;
    ctk::vec2<f32> UV;
};

struct model
{
    ctk::array<vertex> Vertexes;
    ctk::array<u32> Indexes;
};

struct mesh_region
{
    u32 VertexesOffset;
    u32 IndexesOffset;
    u32 IndexCount;
};

struct state
{
    // GLFW
    window Window;

    // Vulkan
    vtk::instance Instance;
    VkSurfaceKHR PlatformSurface;
    vtk::device Device;
    vtk::swapchain Swapchain;
    VkCommandPool GraphicsCommandPool;
    vtk::buffer HostBuffer;
    vtk::buffer DeviceBuffer;
    vtk::region StagingRegion;
    vtk::region VertexRegion;
    vtk::region IndexRegion;
    vtk::image DepthImage;
    vtk::render_pass RenderPass;
    ctk::sarray<VkFramebuffer, 4> Framebuffers;
    ctk::sarray<VkCommandBuffer, 4> CommandBuffers;
    vtk::vertex_layout VertexLayout;
    ctk::map<u32> VertexAttributeIndexes;
    ctk::map<vtk::region> UniformRegions;

    // Assets
    ctk::map<mesh_region> MeshRegions;
    ctk::map<vtk::shader_module> ShaderModules;
};

////////////////////////////////////////////////////////////
/// Internal
////////////////////////////////////////////////////////////
static void
ErrorCallback(s32 Error, cstr Description)
{
    CTK_FATAL("[%d] %s", Error, Description)
}

static void
KeyCallback(GLFWwindow *Window, s32 Key, s32 Scancode, s32 Action, s32 Mods)
{
    auto State = (state *)glfwGetWindowUserPointer(Window);
    // State->KeyDown[Key] = Action == GLFW_PRESS || Action == GLFW_REPEAT;
}

static void
MouseButtonCallback(GLFWwindow *Window, s32 button, s32 Action, s32 Mods)
{
    auto State = (state *)glfwGetWindowUserPointer(Window);
    // State->MouseButtonDown[button] = Action == GLFW_PRESS || Action == GLFW_REPEAT;
}

static void
InitializeGLFWState(state *State)
{
    ctk::data Config = ctk::LoadData("assets/glfw.ctkd");
    glfwSetErrorCallback(ErrorCallback);
    if(glfwInit() != GLFW_TRUE)
    {
        CTK_FATAL("failed to init GLFW")
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow *Window = glfwCreateWindow(ctk::S32(&Config, "window.width"),
                                          ctk::S32(&Config, "window.height"),
                                          ctk::CStr(&Config, "window.title"),
                                          NULL, NULL);
    if(Window == NULL)
    {
        CTK_FATAL("failed to create window")
    }
    glfwSetWindowPos(Window, ctk::S32(&Config, "window.x"), ctk::S32(&Config, "window.y"));
    glfwSetWindowUserPointer(Window, (void *)State);
    // glfwSetFramebufferSizeCallback(Window, FramebufferResizeCallback);
    glfwSetKeyCallback(Window, KeyCallback);
    glfwSetMouseButtonCallback(Window, MouseButtonCallback);
    State->Window.Handle = Window;
}

static void
InitializeVulkanState(state *State)
{
    ctk::data Config = ctk::LoadData("assets/vulkan.ctkd");

    // Instance
    u32 GLFWExtensionCount = 0;
    cstr *GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionCount);
    vtk::instance_config InstanceConfig = {};
    ctk::Push(&InstanceConfig.Extensions, GLFWExtensions, GLFWExtensionCount);
    InstanceConfig.Debug = ctk::B32(&Config, "debug");
    InstanceConfig.AppName = ctk::CStr(&Config, "app_name");
    State->Instance = vtk::CreateInstance(&InstanceConfig);

    // Platform Surface
    VkResult Result = glfwCreateWindowSurface(State->Instance.Handle, State->Window.Handle, NULL, &State->PlatformSurface);
    vtk::ValidateVkResult(Result, "glfwCreateWindowSurface", "failed to create GLFW surface");

    // Device
    vtk::device_config DeviceConfig = {};
    ctk::Push(&DeviceConfig.Extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME); // Swapchains required for rendering.
    DeviceConfig.Features.geometryShader = VK_TRUE;
    DeviceConfig.Features.samplerAnisotropy = VK_TRUE;
    // DeviceConfig.Features.vertexPipelineStoresAndAtomics = VK_TRUE;
    State->Device = vtk::CreateDevice(State->Instance.Handle, State->PlatformSurface, &DeviceConfig);

    // Swapchain
    State->Swapchain = vtk::CreateSwapchain(&State->Device, State->PlatformSurface);

    // Graphics Command Pool
    State->GraphicsCommandPool = vtk::CreateCommandPool(State->Device.Logical, State->Device.QueueFamilyIndexes.Graphics);

    ////////////////////////////////////////////////////////////
    /// Data
    ////////////////////////////////////////////////////////////
    ctk::data *MemoryData = ctk::At(&Config, "memory");

    // Buffers
    vtk::buffer_config HostBufferConfig = {};
    HostBufferConfig.Size = ctk::U32(MemoryData, "host_size");
    HostBufferConfig.UsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    HostBufferConfig.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    State->HostBuffer = vtk::CreateBuffer(&State->Device, &HostBufferConfig);

    vtk::buffer_config DeviceBufferConfig = {};
    DeviceBufferConfig.Size = ctk::U32(MemoryData, "device_size");
    DeviceBufferConfig.UsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    DeviceBufferConfig.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    State->DeviceBuffer = vtk::CreateBuffer(&State->Device, &DeviceBufferConfig);

    // Regions
    State->StagingRegion = vtk::AllocateRegion(&State->HostBuffer, KILOBYTE);

    ////////////////////////////////////////////////////////////
    /// Depth Image
    ////////////////////////////////////////////////////////////
    vtk::image_config DepthImageConfig = {};
    DepthImageConfig.Width = State->Swapchain.Extent.width;
    DepthImageConfig.Height = State->Swapchain.Extent.height;
    DepthImageConfig.Format = vtk::FindDepthImageFormat(State->Device.Physical);
    DepthImageConfig.Tiling = VK_IMAGE_TILING_OPTIMAL;
    DepthImageConfig.UsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    DepthImageConfig.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    DepthImageConfig.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    State->DepthImage = vtk::CreateImage(&State->Device, &DepthImageConfig);
    vtk::TransitionImageLayout(&State->Device, State->GraphicsCommandPool, &State->DepthImage,
                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass_config RenderPassConfig = {};

    // Attachments
    u32 ColorAttachmentIndex = RenderPassConfig.Attachments.Count;
    vtk::attachment *ColorAttachment = ctk::Push(&RenderPassConfig.Attachments);
    ColorAttachment->Description.format = State->Swapchain.ImageFormat;
    ColorAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear color attachment before drawing.
    ColorAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store rendered contents in memory.
    ColorAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Not currently relevant.
    ColorAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Not currently relevant.
    ColorAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Image layout before render pass.
    ColorAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    ColorAttachment->ClearValue = { 0.04f, 0.04f, 0.04f, 1.0f };

    u32 DepthAttachmentIndex = RenderPassConfig.Attachments.Count;
    vtk::attachment *DepthAttachment = ctk::Push(&RenderPassConfig.Attachments);
    DepthAttachment->Description.format = State->DepthImage.Format;
    DepthAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear color attachment before drawing.
    DepthAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Store rendered contents in memory.
    DepthAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Not currently relevant.
    DepthAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Not currently relevant.
    DepthAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Image layout before render pass.
    DepthAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    DepthAttachment->ClearValue = { 1.0f, 0.0f };

    // Subpasses
    vtk::subpass *Subpass = ctk::Push(&RenderPassConfig.Subpasses);

    VkAttachmentReference *ColorAttachmentReference = ctk::Push(&Subpass->ColorAttachmentReferences);
    ColorAttachmentReference->attachment = ColorAttachmentIndex;
    ColorAttachmentReference->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    Subpass->DepthAttachmentReference.Set = true;
    Subpass->DepthAttachmentReference.Value.attachment = DepthAttachmentIndex;
    Subpass->DepthAttachmentReference.Value.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Creation
    State->RenderPass = vtk::CreateRenderPass(State->Device.Logical, &RenderPassConfig);

    // Framebuffers
    for(u32 FramebufferIndex = 0; FramebufferIndex < State->Swapchain.Images.Count; ++FramebufferIndex)
    {
        vtk::framebuffer_config FramebufferConfig = {};
        ctk::Push(&FramebufferConfig.Attachments, State->Swapchain.Images[FramebufferIndex].View);
        ctk::Push(&FramebufferConfig.Attachments, State->DepthImage.View);
        FramebufferConfig.Extent = State->Swapchain.Extent;
        FramebufferConfig.Layers = 1;
        ctk::Push(&State->Framebuffers, vtk::CreateFramebuffer(State->Device.Logical, State->RenderPass.Handle, &FramebufferConfig));
    }

    // Command Buffers
    vtk::AllocateCommandBuffers(State->Device.Logical, State->GraphicsCommandPool, State->Swapchain.Images.Count,
                                State->CommandBuffers.Data);

    ////////////////////////////////////////////////////////////
    /// Vertex Layout
    ////////////////////////////////////////////////////////////
    ctk::data *VertexLayoutData = ctk::At(&Config, "vertex_layout");
    u32 VertexAttributeCount = VertexLayoutData->Children.Count;
    State->VertexAttributeIndexes = ctk::CreateMap<u32>(VertexAttributeCount);
    for(u32 VertexAttributeIndex = 0; VertexAttributeIndex < VertexAttributeCount; ++VertexAttributeIndex)
    {
        ctk::data *VertexAttributeData = ctk::At(VertexLayoutData, VertexAttributeIndex);
        ctk::Push(&State->VertexAttributeIndexes, VertexAttributeData->Key.Data,
                  vtk::PushVertexAttribute(&State->VertexLayout, ctk::U32(VertexAttributeData, "element_count")));
    }

    ////////////////////////////////////////////////////////////
    /// Uniform Regions
    ////////////////////////////////////////////////////////////
    ctk::data *UniformRegionData = ctk::At(&Config, "uniform_regions");
    State->UniformRegions = ctk::CreateMap<vtk::region>(1);
    ctk::Push(&State->UniformRegions, "mvp_matrixes",
              vtk::AllocateRegion(&State->HostBuffer, sizeof(glm::mat4) * ctk::U32(UniformRegionData, "mvp_matrixes.max_elements")));
}

static model
LoadModel(cstr Path)
{
    model Model = {};
    const aiScene *Scene = aiImportFile(Path,
                                        aiProcess_CalcTangentSpace |
                                        aiProcess_Triangulate |
                                        aiProcess_JoinIdenticalVertices |
                                        aiProcess_SortByPType);
    if(Scene == NULL || Scene->mRootNode == NULL || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
    {
        CTK_FATAL("error loading model from path \"%s\": %s", Path, aiGetErrorString())
    }

    // HACK: Bake all meshes from model file into single mesh object.
    ////////////////////////////////////////////////////////////
    /// Allocation
    ////////////////////////////////////////////////////////////
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
    Model.Vertexes = ctk::CreateArrayEmpty<vertex>(VertexCount);
    Model.Indexes = ctk::CreateArrayEmpty<u32>(IndexCount);

    ////////////////////////////////////////////////////////////
    /// Processing
    ////////////////////////////////////////////////////////////
    for(u32 MeshIndex = 0; MeshIndex < Scene->mNumMeshes; ++MeshIndex)
    {
        aiMesh *Mesh = Scene->mMeshes[MeshIndex];
        u32 IndexBase = Model.Vertexes.Count;
        for(u32 vertex_index = 0; vertex_index < Mesh->mNumVertices; ++vertex_index)
        {
            vertex *Vertex = ctk::Push(&Model.Vertexes);
            aiVector3D *Position = Mesh->mVertices + vertex_index;
            aiVector3D *Normal = Mesh->mNormals + vertex_index;
            Vertex->Position = { Position->x, Position->y, Position->z };
            Vertex->Normal = { Normal->x, Normal->y, Normal->z };

            // Texture coordinates are optional.
            if(Mesh->mTextureCoords[0] == NULL)
            {
                Vertex->UV = { 0, 0 };
            }
            else
            {
                aiVector3D *UV = Mesh->mTextureCoords[0] + vertex_index;
                Vertex->UV = { UV->x, UV->y };
            }
        }
        for(u32 FaceIndex = 0; FaceIndex < Mesh->mNumFaces; ++FaceIndex)
        {
            aiFace *Face = Mesh->mFaces + FaceIndex;
            for(u32 IndexIndex = 0; IndexIndex < Face->mNumIndices; ++IndexIndex)
            {
                ctk::Push(&Model.Indexes, IndexBase + Face->mIndices[IndexIndex]);
            }
        }
    }

    // Cleanup
    aiReleaseImport(Scene);

    return Model;
}

static void
Free(model *Model)
{
    ctk::Free(&Model->Vertexes);
    ctk::Free(&Model->Indexes);
}

static void
LoadModels(state *State, ctk::data *AssetData)
{
    // Load models.
    ctk::smap<model, 64> Models = {};
    ctk::data *ModelDataArray = ctk::At(AssetData, "models");
    u32 ModelCount = ModelDataArray->Children.Count;
    CTK_ASSERT(ModelCount <= Models.Size)
    u32 TotalVertexCount = 0;
    u32 TotalIndexCount = 0;
    char FullPath[64] = {};
    static cstr BASE_DIRECTORY = "assets/models";
    for(u32 ModelIndex = 0; ModelIndex < ModelCount; ++ModelIndex)
    {
        cstr ModelName = ctk::CStr(ModelDataArray, ModelIndex);
        sprintf(FullPath, "%s/%s.obj", BASE_DIRECTORY, ModelName);
        model *Model = ctk::Push(&Models, ModelName, LoadModel(FullPath));
        TotalVertexCount += Model->Vertexes.Count;
        TotalIndexCount += Model->Indexes.Count;
    }

    // Allocate regions in device buffer for storing vertex and index data.
    State->VertexRegion = vtk::AllocateRegion(&State->DeviceBuffer, TotalVertexCount * sizeof(vertex));
    State->IndexRegion = vtk::AllocateRegion(&State->DeviceBuffer, TotalIndexCount * sizeof(u32));

    // Generate references to vertex and index data for each model for rendering.
    State->MeshRegions = ctk::CreateMap<mesh_region>(ModelCount);
    u32 VertexOffset = 0;
    u32 IndexOffset = 0;
    for(u32 ModelIndex = 0; ModelIndex < Models.Count; ++ModelIndex)
    {
        cstr ModelKey = Models.Keys[ModelIndex];
        model *Model = ctk::At(&Models, ModelKey);
        u32 ModelVertexesByteSize = ctk::ByteCount(&Model->Vertexes);
        u32 ModelIndexesByteSize = ctk::ByteCount(&Model->Indexes);
        mesh_region *MeshRegion = ctk::Push(&State->MeshRegions, ModelKey);
        MeshRegion->VertexesOffset = VertexOffset;
        MeshRegion->IndexesOffset = IndexOffset;
        MeshRegion->IndexCount = Model->Indexes.Count;
        VertexOffset += ModelVertexesByteSize;
        IndexOffset += ModelIndexesByteSize;
        vtk::WriteToDeviceRegion(&State->Device, State->GraphicsCommandPool, &State->StagingRegion, &State->VertexRegion,
                                 Model->Vertexes.Data, ModelVertexesByteSize, MeshRegion->VertexesOffset);
        vtk::WriteToDeviceRegion(&State->Device, State->GraphicsCommandPool, &State->StagingRegion, &State->IndexRegion,
                                 Model->Indexes.Data, ModelIndexesByteSize, MeshRegion->IndexesOffset);
    }

    // ctk::Info("mesh regions:");
    // for(u32 Index = 0; Index < State->MeshRegions.Count; ++Index)
    // {
    //     mesh_region *MeshRegion = State->MeshRegions.Values + Index;
    //     ctk::Info(1, "[%u] %s: VertexesOffset=%u IndexesOffset=%u IndexCount=%u",
    //               Index, State->MeshRegions.Keys + Index, MeshRegion->VertexesOffset, MeshRegion->IndexesOffset, MeshRegion->IndexCount);
    // }

    // Cleanup
    for(model *Model = Models.Values; Model < Models.Values + Models.Count; ++Model)
    {
        Free(Model);
    }
}

static void
LoadShaderModules(state *State, ctk::data *AssetData)
{
    ctk::data *ShaderDataArray = ctk::At(AssetData, "shaders");
    u32 ShaderCount = ShaderDataArray->Children.Count;
    State->ShaderModules = ctk::CreateMap<vtk::shader_module>(ShaderCount);
    char FullPath[64] = {};
    static cstr BASE_DIRECTORY = "assets/shaders";
    for(u32 ShaderIndex = 0; ShaderIndex < ShaderCount; ++ShaderIndex)
    {
        ctk::data *ShaderData = ctk::At(ShaderDataArray, ShaderIndex);
        cstr ShaderName = ctk::CStr(ShaderData, "name");
        sprintf(FullPath, "%s/%s.spv", BASE_DIRECTORY, ShaderName);
        VkShaderStageFlagBits ShaderStage = vtk::GetVkShaderStageFlagBits(ctk::CStr(ShaderData, "stage"));
        ctk::Push(&State->ShaderModules, ShaderName, vtk::CreateShaderModule(State->Device.Logical, FullPath, ShaderStage));
    }
}

static void
LoadAssets(state *State)
{
    ctk::data AssetData = ctk::LoadData("assets/assets.ctkd");
    LoadModels(State, &AssetData);
    LoadShaderModules(State, &AssetData);

    // ////////////////////////////////////////////////////////////
    // /// Sampler
    // ////////////////////////////////////////////////////////////
    // VkSamplerCreateInfo SamplerCreateInfo = {};
    // SamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // SamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    // SamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    // SamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // SamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // SamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // SamplerCreateInfo.anisotropyEnable = VK_TRUE;
    // SamplerCreateInfo.maxAnisotropy = 16;
    // SamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    // SamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    // SamplerCreateInfo.compareEnable = VK_FALSE;
    // SamplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    // SamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    // SamplerCreateInfo.mipLodBias = 0.0f;
    // SamplerCreateInfo.minLod = 0.0f;
    // SamplerCreateInfo.maxLod = 0.0f;
    // ValidateVkResult(vkCreateSampler(Device->Logical, &SamplerCreateInfo, NULL, &Image.Sampler),
    //                  "vkCreateSampler", "failed to create sampler");
}

////////////////////////////////////////////////////////////
/// Interface
////////////////////////////////////////////////////////////
static b32
WindowClosed(state *State)
{
    return glfwWindowShouldClose(State->Window.Handle);
}

static void
PollEvents(state *State)
{
    glfwPollEvents();
}

static void
Sleep(u32 Milliseconds)
{
    ::Sleep(Milliseconds);
}

} // gfx
