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

struct input_state
{
    b32 KeyDown[GLFW_KEY_LAST + 1];
    b32 MouseButtonDown[GLFW_MOUSE_BUTTON_LAST + 1];
    ctk::vec2<f64> MousePosition = UNSET_MOUSE_POSITION;
    ctk::vec2<f64> MouseDelta;
};

// struct vertex
// {
//     ctk::vec3<f32> Position;
//     ctk::vec3<f32> Normal;
//     ctk::vec2<f32> UV;
// };

// struct model
// {
//     ctk::array<vertex> Vertexes;
//     ctk::array<u32> Indexes;
// };

// struct mesh_region
// {
//     u32 VertexesOffset;
//     u32 IndexesOffset;
//     u32 IndexCount;
// };

// struct asset_state
// {
//     ctk::map<mesh_region> MeshRegions;
//     ctk::map<vtk::shader_module> ShaderModules;
// };

struct graphics_state
{
    vtk::instance Instance;
    VkSurfaceKHR PlatformSurface;
    vtk::device Device;
    vtk::swapchain Swapchain;
    VkCommandPool GraphicsCommandPool;
    vtk::frame_state FrameState;
    vtk::image DepthImage;
    vtk::render_pass RenderPass;
    ctk::sarray<VkFramebuffer, 4> Framebuffers;
    ctk::sarray<VkCommandBuffer, 4> CommandBuffers;
    vtk::vertex_layout VertexLayout;
    vtk::buffer HostBuffer;
    vtk::buffer DeviceBuffer;
    vtk::region StagingRegion;
    vtk::uniform_buffer EntityUniformBuffer;

    // Vulkan State
    VkDescriptorPool DescriptorPool;
    ctk::smap<vtk::texture, 4> Textures;
    ctk::smap<vtk::shader_module, 16> ShaderModules;
    ctk::smap<vtk::uniform_buffer, 4> UniformBuffers;
    ctk::smap<vtk::descriptor_set, 4> DescriptorSets;
    ctk::smap<u32, 4> VertexAttributeIndexes;
    ctk::smap<vtk::graphics_pipeline, 4> GraphicsPipelines;

    // Assets
    ctk::smap<
};

struct vertex
{
    ctk::vec3<f32> Position;
    ctk::vec2<f32> UV;
};

struct entity_ubo
{
    alignas(16) glm::mat4 ModelMatrix;
    alignas(16) glm::mat4 MVPMatrix;
};

struct mesh
{
    ctk::sarray<vertex, 24> Vertexes;
    ctk::sarray<u32, 36> Indexes;
    vtk::region VertexRegion;
    vtk::region IndexRegion;
};

struct render_entity
{
    ctk::sarray<vtk::descriptor_set *, 4> DescriptorSets;
    vtk::graphics_pipeline *GraphicsPipeline;
    mesh *Mesh;
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
    auto InputState = (input_state *)glfwGetWindowUserPointer(Window);
    InputState->KeyDown[Key] = Action == GLFW_PRESS || Action == GLFW_REPEAT;
}

static void
MouseButtonCallback(GLFWwindow *Window, s32 button, s32 Action, s32 Mods)
{
    auto InputState = (input_state *)glfwGetWindowUserPointer(Window);
    InputState->MouseButtonDown[button] = Action == GLFW_PRESS || Action == GLFW_REPEAT;
}

// static model
// LoadModel(cstr Path)
// {
//     model Model = {};
//     u32 ProcessingFlags = aiProcess_CalcTangentSpace |
//                           aiProcess_Triangulate |
//                           aiProcess_JoinIdenticalVertices |
//                           aiProcess_SortByPType;
//     const aiScene *Scene = aiImportFile(Path, ProcessingFlags);
//     if(Scene == NULL || Scene->mRootNode == NULL || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
//     {
//         CTK_FATAL("error loading model from path \"%s\": %s", Path, aiGetErrorString())
//     }

//     // HACK: Bake all meshes from model file into single mesh object.
//     ////////////////////////////////////////////////////////////
//     /// Allocation
//     ////////////////////////////////////////////////////////////
//     u32 VertexCount = 0;
//     u32 IndexCount = 0;
//     for(u32 MeshIndex = 0; MeshIndex < Scene->mNumMeshes; ++MeshIndex)
//     {
//         aiMesh *Mesh = Scene->mMeshes[MeshIndex];
//         VertexCount += Mesh->mNumVertices;
//         for(u32 FaceIndex = 0; FaceIndex < Mesh->mNumFaces; ++FaceIndex)
//         {
//             IndexCount += Mesh->mFaces[FaceIndex].mNumIndices;
//         }
//     }
//     Model.Vertexes = ctk::CreateArrayEmpty<vertex>(VertexCount);
//     Model.Indexes = ctk::CreateArrayEmpty<u32>(IndexCount);

//     ////////////////////////////////////////////////////////////
//     /// Processing
//     ////////////////////////////////////////////////////////////
//     for(u32 MeshIndex = 0; MeshIndex < Scene->mNumMeshes; ++MeshIndex)
//     {
//         aiMesh *Mesh = Scene->mMeshes[MeshIndex];
//         u32 IndexBase = Model.Vertexes.Count;
//         for(u32 vertex_index = 0; vertex_index < Mesh->mNumVertices; ++vertex_index)
//         {
//             vertex *Vertex = ctk::Push(&Model.Vertexes);
//             aiVector3D *Position = Mesh->mVertices + vertex_index;
//             aiVector3D *Normal = Mesh->mNormals + vertex_index;
//             Vertex->Position = { Position->x, Position->y, Position->z };
//             Vertex->Normal = { Normal->x, Normal->y, Normal->z };

//             // Texture coordinates are optional.
//             if(Mesh->mTextureCoords[0] == NULL)
//             {
//                 Vertex->UV = { 0, 0 };
//             }
//             else
//             {
//                 aiVector3D *UV = Mesh->mTextureCoords[0] + vertex_index;
//                 Vertex->UV = { UV->x, UV->y };
//             }
//         }
//         for(u32 FaceIndex = 0; FaceIndex < Mesh->mNumFaces; ++FaceIndex)
//         {
//             aiFace *Face = Mesh->mFaces + FaceIndex;
//             for(u32 IndexIndex = 0; IndexIndex < Face->mNumIndices; ++IndexIndex)
//             {
//                 ctk::Push(&Model.Indexes, IndexBase + Face->mIndices[IndexIndex]);
//             }
//         }
//     }

//     // Cleanup
//     aiReleaseImport(Scene);

//     return Model;
// }

// static void
// Free(model *Model)
// {
//     ctk::Free(&Model->Vertexes);
//     ctk::Free(&Model->Indexes);
// }

// static void
// LoadModels(asset_state *AssetState, ctk::data *Data)
// {
//     // Load models.
//     ctk::smap<model, 64> Models = {};
//     u32 ModelCount = ModelDataArray->Children.Count;
//     CTK_ASSERT(ModelCount <= Models.Size)
//     u32 TotalVertexCount = 0;
//     u32 TotalIndexCount = 0;
//     char FullPath[64] = {};
//     static cstr BASE_DIRECTORY = "assets/models";
//     for(u32 ModelIndex = 0; ModelIndex < ModelCount; ++ModelIndex)
//     {
//         cstr ModelName = ctk::CStr(Data, ModelIndex);
//         sprintf(FullPath, "%s/%s.obj", BASE_DIRECTORY, ModelName);
//         model *Model = ctk::Push(&Models, ModelName, LoadModel(FullPath));
//         TotalVertexCount += Model->Vertexes.Count;
//         TotalIndexCount += Model->Indexes.Count;
//     }

//     // Allocate regions in device buffer for storing vertex and index data.
//     AssetState->VertexRegion = vtk::AllocateRegion(&AssetState->DeviceBuffer, TotalVertexCount, sizeof(vertex));
//     AssetState->IndexRegion = vtk::AllocateRegion(&AssetState->DeviceBuffer, TotalIndexCount, sizeof(u32));

//     // Generate references to vertex and index data for each model for rendering.
//     AssetState->MeshRegions = ctk::CreateMap<mesh_region>(ModelCount);
//     u32 VertexOffset = 0;
//     u32 IndexOffset = 0;
//     for(u32 ModelIndex = 0; ModelIndex < Models.Count; ++ModelIndex)
//     {
//         cstr ModelKey = Models.Keys[ModelIndex];
//         model *Model = ctk::At(&Models, ModelKey);
//         u32 ModelVertexesByteSize = ctk::ByteCount(&Model->Vertexes);
//         u32 ModelIndexesByteSize = ctk::ByteCount(&Model->Indexes);
//         mesh_region *MeshRegion = ctk::Push(&AssetState->MeshRegions, ModelKey);
//         MeshRegion->VertexesOffset = VertexOffset;
//         MeshRegion->IndexesOffset = IndexOffset;
//         MeshRegion->IndexCount = Model->Indexes.Count;
//         VertexOffset += ModelVertexesByteSize;
//         IndexOffset += ModelIndexesByteSize;
//         vtk::WriteToDeviceRegion(&AssetState->Device, AssetState->GraphicsCommandPool, &AssetState->StagingRegion, &AssetState->VertexRegion,
//                                  Model->Vertexes.Data, ModelVertexesByteSize, MeshRegion->VertexesOffset);
//         vtk::WriteToDeviceRegion(&AssetState->Device, AssetState->GraphicsCommandPool, &AssetState->StagingRegion, &AssetState->IndexRegion,
//                                  Model->Indexes.Data, ModelIndexesByteSize, MeshRegion->IndexesOffset);
//     }

//     // ctk::Info("mesh regions:");
//     // for(u32 Index = 0; Index < AssetState->MeshRegions.Count; ++Index)
//     // {
//     //     mesh_region *MeshRegion = AssetState->MeshRegions.Values + Index;
//     //     ctk::Info(1, "[%u] %s: VertexesOffset=%u IndexesOffset=%u IndexCount=%u",
//     //               Index, AssetState->MeshRegions.Keys + Index, MeshRegion->VertexesOffset, MeshRegion->IndexesOffset, MeshRegion->IndexCount);
//     // }

//     // Cleanup
//     for(model *Model = Models.Values; Model < Models.Values + Models.Count; ++Model)
//     {
//         Free(Model);
//     }
// }

// static void
// LoadShaderModules(asset_state *AssetState, ctk::data *Data)
// {
//     u32 ShaderCount = Data->Children.Count;
//     AssetState->ShaderModules = ctk::CreateMap<vtk::shader_module>(ShaderCount);
//     char FullPath[64] = {};
//     static cstr BASE_DIRECTORY = "assets/shaders";
//     for(u32 ShaderIndex = 0; ShaderIndex < ShaderCount; ++ShaderIndex)
//     {
//         ctk::data *ShaderData = ctk::At(Data, ShaderIndex);
//         cstr ShaderName = ShaderData->Key.Data;
//         sprintf(FullPath, "%s/%s.spv", BASE_DIRECTORY, ShaderName);
//         VkShaderStageFlagBits ShaderStage = vtk::GetVkShaderStageFlagBits(ctk::CStr(ShaderData, "stage"));
//         ctk::Push(&AssetState->ShaderModules, ShaderName, vtk::CreateShaderModule(AssetState->Device.Logical, FullPath, ShaderStage));
//     }
// }

static void
InitVulkan(graphics_state *GraphicsState, window *Window, ctk::data *VulkanData)
{
    vtk::instance *Instance = &GraphicsState->Instance;
    VkSurfaceKHR *PlatformSurface = &GraphicsState->PlatformSurface;
    vtk::device *Device = &GraphicsState->Device;
    vtk::swapchain *Swapchain = &GraphicsState->Swapchain;
    VkCommandPool *GraphicsCommandPool = &GraphicsState->GraphicsCommandPool;
    vtk::frame_state *FrameState = &GraphicsState->FrameState;
    vtk::image *DepthImage = &GraphicsState->DepthImage;
    vtk::render_pass *RenderPass = &GraphicsState->RenderPass;
    ctk::sarray<VkFramebuffer, 4> *Framebuffers = &GraphicsState->Framebuffers;
    ctk::sarray<VkCommandBuffer, 4> *CommandBuffers = &GraphicsState->CommandBuffers;
    ctk::smap<vtk::descriptor_set, 4> *DescriptorSets = &GraphicsState->DescriptorSets;
    vtk::buffer *HostBuffer = &GraphicsState->HostBuffer;
    vtk::buffer *DeviceBuffer = &GraphicsState->DeviceBuffer;
    vtk::region *StagingRegion = &GraphicsState->StagingRegion;

    // Instance
    u32 GLFWExtensionCount = 0;
    cstr *GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionCount);
    vtk::instance_info InstanceInfo = {};
    ctk::Push(&InstanceInfo.Extensions, GLFWExtensions, GLFWExtensionCount);
    InstanceInfo.Debug = ctk::B32(VulkanData, "debug");
    InstanceInfo.AppName = ctk::CStr(VulkanData, "app_name");
    *Instance = vtk::CreateInstance(&InstanceInfo);

    // Platform Surface
    vtk::ValidateVkResult(glfwCreateWindowSurface(Instance->Handle, Window->Handle, NULL, PlatformSurface),
                          "glfwCreateWindowSurface", "failed to create GLFW surface");

    // Device
    vtk::device_info DeviceInfo = {};
    Push(&DeviceInfo.Extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME); // Swapchains required for rendering.
    DeviceInfo.Features.geometryShader = VK_TRUE;
    DeviceInfo.Features.samplerAnisotropy = VK_TRUE;
    // DeviceInfo.Features.vertexPipelineStoresAndAtomics = VK_TRUE;
    *Device = vtk::CreateDevice(Instance->Handle, *PlatformSurface, &DeviceInfo);

    // Swapchain
    *Swapchain = vtk::CreateSwapchain(Device, *PlatformSurface);

    // Graphics Command Pool
    *GraphicsCommandPool = vtk::CreateCommandPool(Device->Logical, Device->QueueFamilyIndexes.Graphics);

    // Frame State
    *FrameState = vtk::CreateFrameState(Device->Logical, 2, Swapchain->Images.Count);

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
    vtk::TransitionImageLayout(Device, *GraphicsCommandPool, DepthImage,
                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass_info RenderPassInfo = {};

    // Attachments
    u32 ColorAttachmentIndex = RenderPassInfo.Attachments.Count;
    vtk::attachment *ColorAttachment = ctk::Push(&RenderPassInfo.Attachments);
    ColorAttachment->Description.format = Swapchain->ImageFormat;
    ColorAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear color attachment before drawing.
    ColorAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store rendered contents in memory.
    ColorAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Not currently relevant.
    ColorAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Not currently relevant.
    ColorAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Image layout before render pass.
    ColorAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    ColorAttachment->ClearValue = { 0.04f, 0.04f, 0.04f, 1.0f };

    u32 DepthAttachmentIndex = RenderPassInfo.Attachments.Count;
    vtk::attachment *DepthAttachment = ctk::Push(&RenderPassInfo.Attachments);
    DepthAttachment->Description.format = DepthImage->Format;
    DepthAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear color attachment before drawing.
    DepthAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Store rendered contents in memory.
    DepthAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Not currently relevant.
    DepthAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Not currently relevant.
    DepthAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Image layout before render pass.
    DepthAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    DepthAttachment->ClearValue = { 1.0f, 0.0f };

    // Subpasses
    vtk::subpass *Subpass = ctk::Push(&RenderPassInfo.Subpasses);

    VkAttachmentReference *ColorAttachmentReference = ctk::Push(&Subpass->ColorAttachmentReferences);
    ColorAttachmentReference->attachment = ColorAttachmentIndex;
    ColorAttachmentReference->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    Subpass->DepthAttachmentReference.Set = true;
    Subpass->DepthAttachmentReference.Value.attachment = DepthAttachmentIndex;
    Subpass->DepthAttachmentReference.Value.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Creation
    *RenderPass = vtk::CreateRenderPass(Device->Logical, &RenderPassInfo);

    // Framebuffers
    for(u32 FramebufferIndex = 0; FramebufferIndex < Swapchain->Images.Count; ++FramebufferIndex)
    {
        vtk::framebuffer_info FramebufferInfo = {};
        ctk::Push(&FramebufferInfo.Attachments, Swapchain->Images[FramebufferIndex].View);
        ctk::Push(&FramebufferInfo.Attachments, DepthImage->View);
        FramebufferInfo.Extent = Swapchain->Extent;
        FramebufferInfo.Layers = 1;
        ctk::Push(Framebuffers, vtk::CreateFramebuffer(Device->Logical, RenderPass->Handle, &FramebufferInfo));
    }

    // Command Buffers
    CommandBuffers->Count = Swapchain->Images.Count;
    vtk::AllocateCommandBuffers(Device->Logical, *GraphicsCommandPool, Swapchain->Images.Count, CommandBuffers->Data);

    ////////////////////////////////////////////////////////////
    /// Memory
    ////////////////////////////////////////////////////////////

    // Buffers
    vtk::buffer_info HostBufferInfo = {};
    HostBufferInfo.Size = 10 * MEGABYTE;
    HostBufferInfo.UsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    HostBufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    *HostBuffer = vtk::CreateBuffer(Device, &HostBufferInfo);

    vtk::buffer_info DeviceBufferInfo = {};
    DeviceBufferInfo.Size = 10 * MEGABYTE;
    DeviceBufferInfo.UsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    DeviceBufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    *DeviceBuffer = vtk::CreateBuffer(Device, &DeviceBufferInfo);

    // Regions
    *StagingRegion = vtk::AllocateRegion(HostBuffer, 2 * MEGABYTE);
}

static void
LoadVulkanState(graphics_state *GraphicsState, ctk::data *VulkanData)
{
    vtk::device *Device = &GraphicsState->Device;
    vtk::swapchain *Swapchain = &GraphicsState->Swapchain;
    vtk::render_pass *RenderPass = &GraphicsState->RenderPass;
    vtk::frame_state *FrameState = &GraphicsState->FrameState;
    vtk::region *StagingRegion = &GraphicsState->StagingRegion;
    VkCommandPool *GraphicsCommandPool = &GraphicsState->GraphicsCommandPool;
    vtk::vertex_layout *VertexLayout = &GraphicsState->VertexLayout;
    VkDescriptorPool *DescriptorPool = &GraphicsState->DescriptorPool;
    ctk::smap<vtk::texture, 4> *Textures = &GraphicsState->Textures;
    ctk::smap<vtk::shader_module, 16> *ShaderModules = &GraphicsState->ShaderModules;
    ctk::smap<vtk::uniform_buffer, 4> *UniformBuffers = &GraphicsState->UniformBuffers;
    ctk::smap<vtk::descriptor_set, 4> *DescriptorSets = &GraphicsState->DescriptorSets;
    ctk::smap<u32, 4> *VertexAttributeIndexes = &GraphicsState->VertexAttributeIndexes;
    ctk::smap<vtk::graphics_pipeline, 4> *GraphicsPipelines = &GraphicsState->GraphicsPipelines;

    ////////////////////////////////////////////////////////////
    /// Textures
    ////////////////////////////////////////////////////////////
    ctk::data *TextureMap = ctk::At(VulkanData, "textures");
    for(u32 TextureIndex = 0; TextureIndex < TextureMap->Children.Count; ++TextureIndex)
    {
        ctk::data *TextureData = ctk::At(TextureMap, TextureIndex);
        vtk::texture_info TextureInfo = {};
        TextureInfo.Filter = vtk::GetVkFilter(ctk::CStr(TextureData, "filter"));
        ctk::Push(Textures, TextureData->Key.Data,
                  vtk::LoadTexture(Device, *GraphicsCommandPool, StagingRegion, ctk::CStr(TextureData, "path"), &TextureInfo));
    }

    ////////////////////////////////////////////////////////////
    /// Shader Modules
    ////////////////////////////////////////////////////////////
    ctk::data *ShaderModuleMap = ctk::At(VulkanData, "shader_modules");
    for(u32 ShaderModuleIndex = 0; ShaderModuleIndex < ShaderModuleMap->Children.Count; ++ShaderModuleIndex)
    {
        ctk::data *ShaderModuleData = ctk::At(ShaderModuleMap, ShaderModuleIndex);
        VkShaderStageFlagBits Stage = vtk::GetVkShaderStageFlagBits(ctk::CStr(ShaderModuleData, "stage"));
        ctk::Push(ShaderModules, ShaderModuleData->Key.Data,
                  vtk::CreateShaderModule(Device->Logical, ctk::CStr(ShaderModuleData, "path"), Stage));
    }

    ////////////////////////////////////////////////////////////
    /// Descriptor Sets
    ////////////////////////////////////////////////////////////

    // Descriptor Infos
    ctk::smap<vtk::descriptor_info, 8> DescriptorInfos = {};
    ctk::data *DescriptorMap = ctk::At(VulkanData, "descriptors");
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
            DescriptorInfo->Texture = ctk::At(Textures, ctk::CStr(DescriptorData, "texture"));
        }
        else
        {
            CTK_FATAL("unhandled descriptor type when loading descriptors")
        }
    }

    // Descriptor Set Infos
    ctk::smap<vtk::descriptor_set_info, 8> DescriptorSetInfos = {};
    ctk::data *DescriptorSetMap = ctk::At(VulkanData, "descriptor_sets");
    for(u32 DescriptorSetIndex = 0; DescriptorSetIndex < DescriptorSetMap->Children.Count; ++DescriptorSetIndex)
    {
        ctk::data *DescriptorSetData = ctk::At(DescriptorSetMap, DescriptorSetIndex);
        vtk::descriptor_set_info *DescriptorSetInfo = ctk::Push(&DescriptorSetInfos, DescriptorSetData->Key.Data);
        b32 IsDynamic = ctk::StringEqual(ctk::CStr(DescriptorSetData, "type"), "dynamic");
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
    ctk::data *VertexLayoutMap = ctk::At(VulkanData, "vertex_layout");
    for(u32 VertexAttributeIndex = 0; VertexAttributeIndex < VertexLayoutMap->Children.Count; ++VertexAttributeIndex)
    {
        ctk::data *VertexAttributeData = ctk::At(VertexLayoutMap, VertexAttributeIndex);
        ctk::Push(VertexAttributeIndexes, VertexAttributeData->Key.Data,
                  vtk::PushVertexAttribute(&GraphicsState->VertexLayout, ctk::U32(VertexAttributeData, "element_count")));
    }

    ////////////////////////////////////////////////////////////
    /// Graphics Pipelines
    ////////////////////////////////////////////////////////////
    ctk::data *GraphicsPipelineMap = ctk::At(VulkanData, "graphics_pipelines");
    for(u32 GraphicsPipelineIndex = 0; GraphicsPipelineIndex < GraphicsPipelineMap->Children.Count; ++GraphicsPipelineIndex)
    {
        ctk::data *GraphicsPipelineData = ctk::At(GraphicsPipelineMap, GraphicsPipelineIndex);
        ctk::data *ShaderModuleArray = ctk::At(GraphicsPipelineData, "shader_modules");
        ctk::data *DescriptorSetLayoutArray = ctk::At(GraphicsPipelineData, "descriptor_set_layouts");
        ctk::data *VertexInputArray = ctk::At(GraphicsPipelineData, "vertex_inputs");
        vtk::graphics_pipeline_info GraphicsPipelineInfo = {};
        for(u32 ShaderModuleIndex = 0; ShaderModuleIndex < ShaderModuleArray->Children.Count; ++ShaderModuleIndex)
        {
            ctk::Push(&GraphicsPipelineInfo.ShaderModules, ctk::At(ShaderModules, ctk::CStr(ShaderModuleArray, ShaderModuleIndex)));
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
        GraphicsPipelineInfo.ViewportExtent = Swapchain->Extent;
        GraphicsPipelineInfo.PrimitiveTopology = vtk::GetVkPrimitiveTopology(ctk::CStr(GraphicsPipelineData, "primitive_topology"));
        GraphicsPipelineInfo.DepthTesting = vtk::GetVkBool32(ctk::CStr(GraphicsPipelineData, "depth_testing"));;
        ctk::Push(GraphicsPipelines, GraphicsPipelineData->Key.Data,
                  vtk::CreateGraphicsPipeline(Device->Logical, RenderPass->Handle, &GraphicsPipelineInfo));
    }
}

////////////////////////////////////////////////////////////
/// Interface
////////////////////////////////////////////////////////////
static window *
CreateWindow_(input_state *InputState)
{
    window *Window = ctk::Alloc<window>();
    *Window = {};
    ctk::data Data = ctk::LoadData("assets/window.ctkd");
    glfwSetErrorCallback(ErrorCallback);
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
    glfwSetKeyCallback(Window->Handle, KeyCallback);
    glfwSetMouseButtonCallback(Window->Handle, MouseButtonCallback);
    return Window;
}

// static asset_state *
// CreateAssetState()
// {
//     asset_state *AssetState = ctk::Alloc<asset_state>();
//     *AssetState = {};
//     ctk::data AssetData = ctk::LoadData("assets/assets.ctkd");
//     LoadShaderModules(AssetState, ctk::At(AssetData, "shaders"));
//     LoadModels(AssetState, ctk::At(AssetData, "models"));
//     return AssetState;
// }

static graphics_state *
CreateGraphicsState(window *Window, input_state *InputState)
{
    graphics_state *GraphicsState = ctk::Alloc<graphics_state>();
    *GraphicsState = {};
    vtk::device *Device = &GraphicsState->Device;
    vtk::swapchain *Swapchain = &GraphicsState->Swapchain;
    vtk::frame_state *FrameState = &GraphicsState->FrameState;
    VkCommandPool *GraphicsCommandPool = &GraphicsState->GraphicsCommandPool;
    vtk::render_pass *RenderPass = &GraphicsState->RenderPass;
    ctk::sarray<VkFramebuffer, 4> *Framebuffers = &GraphicsState->Framebuffers;
    ctk::sarray<VkCommandBuffer, 4> *CommandBuffers = &GraphicsState->CommandBuffers;
    ctk::smap<vtk::graphics_pipeline, 4> *GraphicsPipelines = &GraphicsState->GraphicsPipelines;
    ctk::smap<vtk::descriptor_set, 4> *DescriptorSets = &GraphicsState->DescriptorSets;
    vtk::buffer *HostBuffer = &GraphicsState->HostBuffer;
    vtk::buffer *DeviceBuffer = &GraphicsState->DeviceBuffer;
    vtk::region *StagingRegion = &GraphicsState->StagingRegion;
    ctk::smap<vtk::uniform_buffer, 4> *UniformBuffers = &GraphicsState->UniformBuffers;
    ctk::data VulkanData = ctk::LoadData("assets/vulkan.ctkd");
    InitVulkan(GraphicsState, Window, &VulkanData);

    // Uniform Buffers
    ctk::Push(UniformBuffers, "entity", vtk::CreateUniformBuffer(HostBuffer, 2, sizeof(entity_ubo), FrameState->Frames.Count));

    LoadVulkanState(GraphicsState, &VulkanData);

    ////////////////////////////////////////////////////////////
    /// Data
    ////////////////////////////////////////////////////////////

    // Meshes
    mesh Meshes[2] = {};
    mesh *QuadMesh = Meshes + 0;
    mesh *CubeMesh = Meshes + 1;
    u32 QuadIndexes[] = { 0, 1, 2, 0, 2, 3 };
    u32 CubeIndexes[] =
    {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
    };
    ctk::Push(&QuadMesh->Vertexes, { { 0.0f,  0.0f, 0.0f }, { 0.0f, 1.0f } });
    ctk::Push(&QuadMesh->Vertexes, { { 1.0f,  0.0f, 0.0f }, { 1.0f, 1.0f } });
    ctk::Push(&QuadMesh->Vertexes, { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } });
    ctk::Push(&QuadMesh->Vertexes, { { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } });
    ctk::Push(&QuadMesh->Indexes, QuadIndexes, CTK_ARRAY_COUNT(QuadIndexes));
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f,  0.0f, 0.0f }, { 0.0f, 1.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 1.0f,  0.0f, 0.0f }, { 1.0f, 1.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f,  0.0f, 1.0f }, { 0.0f, 1.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f,  0.0f, 0.0f }, { 1.0f, 1.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } });
    ctk::Push(&CubeMesh->Vertexes, { { 0.0f, -1.0f, 1.0f }, { 0.0f, 0.0f } });
    ctk::Push(&CubeMesh->Indexes, CubeIndexes, CTK_ARRAY_COUNT(CubeIndexes));
    for(u32 MeshIndex = 0; MeshIndex < CTK_ARRAY_COUNT(Meshes); ++MeshIndex)
    {
        mesh *Mesh = Meshes + MeshIndex;
        u32 VertexByteCount = ctk::ByteCount(&Mesh->Vertexes);
        u32 IndexByteCount = ctk::ByteCount(&Mesh->Indexes);
        Mesh->VertexRegion = vtk::AllocateRegion(DeviceBuffer, VertexByteCount);
        Mesh->IndexRegion = vtk::AllocateRegion(DeviceBuffer, IndexByteCount);
        vtk::WriteToDeviceRegion(Device, *GraphicsCommandPool, StagingRegion, &Mesh->VertexRegion,
                                 Mesh->Vertexes.Data, VertexByteCount, 0);
        vtk::WriteToDeviceRegion(Device, *GraphicsCommandPool, StagingRegion, &Mesh->IndexRegion,
                                 Mesh->Indexes.Data, IndexByteCount, 0);
    }

    ////////////////////////////////////////////////////////////
    /// Scene
    ////////////////////////////////////////////////////////////
    ctk::sarray<render_entity, 4> RenderEntities = {};
    render_entity *QuadEntity = ctk::Push(&RenderEntities);
    ctk::Push(&QuadEntity->DescriptorSets, At(DescriptorSets, "entity"));
    ctk::Push(&QuadEntity->DescriptorSets, At(DescriptorSets, "grass_texture"));
    QuadEntity->GraphicsPipeline = ctk::At(GraphicsPipelines, "default");
    QuadEntity->Mesh = QuadMesh;

    render_entity *CubeEntity = ctk::Push(&RenderEntities);
    ctk::Push(&CubeEntity->DescriptorSets, At(DescriptorSets, "entity"));
    ctk::Push(&CubeEntity->DescriptorSets, At(DescriptorSets, "dirt_texture"));
    CubeEntity->GraphicsPipeline = ctk::At(GraphicsPipelines, "default");
    CubeEntity->Mesh = CubeMesh;

    ////////////////////////////////////////////////////////////
    /// Record render pass.
    ////////////////////////////////////////////////////////////
    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = Swapchain->Extent;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = 0;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

    for(u32 FrameIndex = 0; FrameIndex < FrameState->Frames.Count; ++FrameIndex)
    {
        VkCommandBuffer CommandBuffer = *At(CommandBuffers, FrameIndex);
        vtk::ValidateVkResult(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo),
                              "vkBeginCommandBuffer", "failed to begin recording command buffer");
        VkRenderPassBeginInfo RenderPassBeginInfo = {};
        RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassBeginInfo.renderPass = RenderPass->Handle;
        RenderPassBeginInfo.framebuffer = *At(Framebuffers, FrameIndex);
        RenderPassBeginInfo.renderArea = RenderArea;
        RenderPassBeginInfo.clearValueCount = RenderPass->ClearValues.Count;
        RenderPassBeginInfo.pClearValues = RenderPass->ClearValues.Data;

        // Begin
        vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Render Commands
        for(u32 EntityIndex = 0; EntityIndex < RenderEntities.Count; ++EntityIndex)
        {
            render_entity *RenderEntity = RenderEntities + EntityIndex;
            mesh *Mesh = RenderEntity->Mesh;

            // Graphics Pipeline
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RenderEntity->GraphicsPipeline->Handle);

            // Descriptor Sets
            vtk::BindDescriptorSets(CommandBuffer, RenderEntity->GraphicsPipeline->Layout,
                                    RenderEntity->DescriptorSets.Data, RenderEntity->DescriptorSets.Count,
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

    ////////////////////////////////////////////////////////////
    /// Main Loop
    ////////////////////////////////////////////////////////////
    b32 Close = false;
    glm::vec3 CameraPosition = { 0.0f, 0.0f, -1.0f };
    glm::vec3 CameraRotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 EntityPositions[2] =
    {
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 3.0f },
    };
    while(!glfwWindowShouldClose(Window->Handle) && !Close)
    {
        ////////////////////////////////////////////////////////////
        /// Input
        ////////////////////////////////////////////////////////////
        glfwPollEvents();
        if(InputState->KeyDown[GLFW_KEY_ESCAPE])
        {
            Close = true;
        }

        // Mouse Delta
        ctk::vec2 PreviousMousePosition = InputState->MousePosition;
        f64 CurrentMouseX = 0.0;
        f64 CurrentMouseY = 0.0;
        glfwGetCursorPos(Window->Handle, &CurrentMouseX, &CurrentMouseY);
        InputState->MousePosition = { CurrentMouseX, CurrentMouseY };

        // Calculate delta if previous position was not unset.
        if(PreviousMousePosition != UNSET_MOUSE_POSITION)
        {
            InputState->MouseDelta = InputState->MousePosition - PreviousMousePosition;
        }

        ////////////////////////////////////////////////////////////
        /// Camera Controls
        ////////////////////////////////////////////////////////////

        // Rotation
        if(InputState->MouseButtonDown[GLFW_MOUSE_BUTTON_2])
        {
            static const f32 SENS = 0.4f;
            CameraRotation.x += InputState->MouseDelta.Y * SENS;
            CameraRotation.y -= InputState->MouseDelta.X * SENS;
            CameraRotation.x = ctk::Clamp(CameraRotation.x, -80.0f, 80.0f);
        }

        // Translation
        glm::vec3 Translation = {};
        f32 Modifier = InputState->KeyDown[GLFW_KEY_LEFT_SHIFT] ? 4 :
                       InputState->KeyDown[GLFW_KEY_LEFT_CONTROL] ? 1 :
                       2;
        if(InputState->KeyDown[GLFW_KEY_W]) Translation.z += 0.01f * Modifier;
        if(InputState->KeyDown[GLFW_KEY_S]) Translation.z -= 0.01f * Modifier;
        if(InputState->KeyDown[GLFW_KEY_D]) Translation.x += 0.01f * Modifier;
        if(InputState->KeyDown[GLFW_KEY_A]) Translation.x -= 0.01f * Modifier;
        if(InputState->KeyDown[GLFW_KEY_E]) Translation.y -= 0.01f * Modifier;
        if(InputState->KeyDown[GLFW_KEY_Q]) Translation.y += 0.01f * Modifier;

        glm::mat4 CameraWorldMatrix(1.0f);
        CameraWorldMatrix = glm::rotate(CameraWorldMatrix, glm::radians(CameraRotation.x), { 1.0f, 0.0f, 0.0f });
        CameraWorldMatrix = glm::rotate(CameraWorldMatrix, glm::radians(CameraRotation.y), { 0.0f, 1.0f, 0.0f });
        CameraWorldMatrix = glm::rotate(CameraWorldMatrix, glm::radians(CameraRotation.z), { 0.0f, 0.0f, 1.0f });
        CameraWorldMatrix = glm::translate(CameraWorldMatrix, { CameraPosition.x, CameraPosition.y, CameraPosition.z });

        glm::vec3 Right = {};
        Right.x = CameraWorldMatrix[0][0];
        Right.y = CameraWorldMatrix[1][0];
        Right.z = CameraWorldMatrix[2][0];

        glm::vec3 Up = {};
        Up.x = CameraWorldMatrix[0][1];
        Up.y = CameraWorldMatrix[1][1];
        Up.z = CameraWorldMatrix[2][1];

        glm::vec3 Forward = {};
        Forward.x = CameraWorldMatrix[0][2];
        Forward.y = CameraWorldMatrix[1][2];
        Forward.z = CameraWorldMatrix[2][2];

        glm::vec3 NewPosition = CameraPosition;
        NewPosition = NewPosition + (Right * Translation.x);
        NewPosition = NewPosition + (Up * Translation.y);
        CameraPosition = NewPosition + (Forward * Translation.z);

        ////////////////////////////////////////////////////////////
        /// Update Uniform Data
        ////////////////////////////////////////////////////////////
        entity_ubo EntityUBOs[2] = {};

        // View Matrix
        glm::mat4 CameraMatrix(1.0f);
        CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraRotation.x), { 1.0f, 0.0f, 0.0f });
        CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraRotation.y), { 0.0f, 1.0f, 0.0f });
        CameraMatrix = glm::rotate(CameraMatrix, glm::radians(CameraRotation.z), { 0.0f, 0.0f, 1.0f });
        CameraMatrix = glm::translate(CameraMatrix, CameraPosition);
        glm::vec3 CameraForward = { CameraMatrix[0][2], CameraMatrix[1][2], CameraMatrix[2][2] };
        glm::mat4 ViewMatrix = glm::lookAt(CameraPosition, CameraPosition + CameraForward, { 0.0f, -1.0f, 0.0f });

        // Projection Matrix
        f32 Aspect = Swapchain->Extent.width / (f32)Swapchain->Extent.height;
        glm::mat4 ProjectionMatrix = glm::perspective(glm::radians(90.0f), Aspect, 0.1f, 1000.0f);
        ProjectionMatrix[1][1] *= -1; // Flip y value for scale (glm is designed for OpenGL).

        // Entity Model Matrixes
        for(u32 EntityIndex = 0; EntityIndex < 2; ++EntityIndex)
        {
            glm::mat4 ModelMatrix(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, EntityPositions[EntityIndex]);
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(0.0f), { 1.0f, 0.0f, 0.0f });
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(0.0f), { 0.0f, 1.0f, 0.0f });
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(0.0f), { 0.0f, 0.0f, 1.0f });
            ModelMatrix = glm::scale(ModelMatrix, { 1.0f, 1.0f, 1.0f });
            EntityUBOs[EntityIndex].ModelMatrix = ModelMatrix;
            EntityUBOs[EntityIndex].MVPMatrix = ProjectionMatrix * ViewMatrix * ModelMatrix;
        }
        vtk::WriteToHostRegion(Device->Logical, ctk::At(UniformBuffers, "entity")->Regions + FrameState->CurrentFrameIndex,
                               EntityUBOs, sizeof(EntityUBOs), 0);

        ////////////////////////////////////////////////////////////
        /// Rendering
        ////////////////////////////////////////////////////////////
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
        VkCommandBuffer QueueSubmitCommandBuffers[] = { *At(CommandBuffers, SwapchainImageIndex) };

        VkSubmitInfo SubmitInfos[1] = {};
        SubmitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfos[0].waitSemaphoreCount = CTK_ARRAY_COUNT(QueueSubmitWaitSemaphores);
        SubmitInfos[0].pWaitSemaphores = QueueSubmitWaitSemaphores;
        SubmitInfos[0].pWaitDstStageMask = QueueSubmitWaitStages;
        SubmitInfos[0].commandBufferCount = CTK_ARRAY_COUNT(QueueSubmitCommandBuffers);
        SubmitInfos[0].pCommandBuffers = QueueSubmitCommandBuffers;
        SubmitInfos[0].signalSemaphoreCount = CTK_ARRAY_COUNT(QueueSubmitSignalSemaphores);
        SubmitInfos[0].pSignalSemaphores = QueueSubmitSignalSemaphores;
        {
            // Submit render pass commands to graphics queue for rendering.
            // Signal current frame's in flight flence when commands have finished executing.
            VkResult Result = vkQueueSubmit(Device->GraphicsQueue, CTK_ARRAY_COUNT(SubmitInfos), SubmitInfos, CurrentFrame->InFlightFence);
            vtk::ValidateVkResult(Result, "vkQueueSubmit", "failed to submit command buffer to graphics queue");
        }

        ////////////////////////////////////////////////////////////
        /// Presentation
        ////////////////////////////////////////////////////////////

        // These are parallel; provide 1:1 index per swapchain.
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
        {
            // Submit Swapchains to present queue for presentation once rendering is complete.
            VkResult Result = vkQueuePresentKHR(Device->PresentQueue, &PresentInfo);
            vtk::ValidateVkResult(Result, "vkQueuePresentKHR", "failed to queue image for presentation");
        }

        // Cycle frame.
        FrameState->CurrentFrameIndex = (FrameState->CurrentFrameIndex + 1) % FrameState->Frames.Count;

        Sleep(1);
    }

    return GraphicsState;
}

} // gfx
