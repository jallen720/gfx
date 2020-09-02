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

#include "gfx/imgui/imgui.h"
#include "gfx/imgui/imgui_impl_glfw.h"
#include "gfx/imgui/imgui_impl_vulkan.h"

////////////////////////////////////////////////////////////
/// Constants
////////////////////////////////////////////////////////////
static const ctk::vec2<f64> UNSET_MOUSE_POSITION = { -10000.0, -10000.0 };

////////////////////////////////////////////////////////////
/// Data
////////////////////////////////////////////////////////////
struct input_state {
    b32 KeyDown[GLFW_KEY_LAST + 1];
    b32 MouseButtonDown[GLFW_MOUSE_BUTTON_LAST + 1];
    ctk::vec2<f64> MousePosition = UNSET_MOUSE_POSITION;
    ctk::vec2<f64> MouseDelta;
};

struct vulkan_instance {
    vtk::instance Instance;
    VkSurfaceKHR PlatformSurface;
    vtk::device Device;
    vtk::swapchain Swapchain;
    VkCommandPool GraphicsCommandPool;
    vtk::frame_state FrameState;
    vtk::buffer HostBuffer;
    vtk::buffer DeviceBuffer;
    vtk::region StagingRegion;
};

struct vertex {
    ctk::vec3<f32> Position;
    ctk::vec3<f32> Normal;
    ctk::vec2<f32> UV;
};

struct mesh {
    ctk::array<vertex> Vertexes;
    ctk::array<u32> Indexes;
    vtk::region VertexRegion;
    vtk::region IndexRegion;
};

struct assets {
    ctk::smap<mesh, 32> Meshes;
    ctk::smap<vtk::texture, 32> Textures;
    ctk::smap<vtk::shader_module, 32> ShaderModules;
};

struct transform {
    ctk::vec3<f32> Position;
    ctk::vec3<f32> Rotation;
    ctk::vec3<f32> Scale;
};

struct camera {
    transform Transform;
    f32 FieldOfView;
    f32 Aspect;
    f32 ZNear;
    f32 ZFar;
};

struct ui {
    vtk::render_pass RenderPass;
    VkDescriptorPool DescriptorPool;
    ImGuiIO* IO;
};

////////////////////////////////////////////////////////////
/// Internal
////////////////////////////////////////////////////////////
static void local_translate(transform* Transform, ctk::vec3<f32> Translation) {
    ctk::vec3<f32>* Position = &Transform->Position;
    ctk::vec3<f32>* Rotation = &Transform->Rotation;

    glm::mat4 WorldMatrix(1.0f);
    WorldMatrix = glm::rotate(WorldMatrix, glm::radians(Rotation->X), { 1.0f, 0.0f, 0.0f });
    WorldMatrix = glm::rotate(WorldMatrix, glm::radians(Rotation->Y), { 0.0f, 1.0f, 0.0f });
    WorldMatrix = glm::rotate(WorldMatrix, glm::radians(Rotation->Z), { 0.0f, 0.0f, 1.0f });
    WorldMatrix = glm::translate(WorldMatrix, { Position->X, Position->Y, Position->Z });

    ctk::vec3<f32> Right = {};
    Right.X = WorldMatrix[0][0];
    Right.Y = WorldMatrix[1][0];
    Right.Z = WorldMatrix[2][0];

    ctk::vec3<f32> Up = {};
    Up.X = WorldMatrix[0][1];
    Up.Y = WorldMatrix[1][1];
    Up.Z = WorldMatrix[2][1];

    ctk::vec3<f32> Forward = {};
    Forward.X = WorldMatrix[0][2];
    Forward.Y = WorldMatrix[1][2];
    Forward.Z = WorldMatrix[2][2];

    ctk::vec3<f32> NewPosition = *Position;
    NewPosition = NewPosition + (Right * Translation.X);
    NewPosition = NewPosition + (Up * Translation.Y);
    *Position = NewPosition + (Forward * Translation.Z);
}

static ctk::vec3<f32> load_vec3(ctk::data* Data) {
    return { ctk::to_f32(Data, 0u), ctk::to_f32(Data, 1u), ctk::to_f32(Data, 2u) };
}

static ctk::vec4<f32> load_vec4(ctk::data* Data) {
    return { ctk::to_f32(Data, 0u), ctk::to_f32(Data, 1u), ctk::to_f32(Data, 2u), ctk::to_f32(Data, 3u) };
}

static transform default_transform() {
    return { {}, {}, { 1, 1, 1 } };
}

static transform load_transform(ctk::data* Data) {
    transform Transform = default_transform();
    ctk::data* ChildData = NULL;
    if(ChildData = ctk::find(Data, "position")) {
        Transform.Position = load_vec3(ChildData);
    }
    if(ChildData = ctk::find(Data, "rotation")) {
        Transform.Rotation = load_vec3(ChildData);
    }
    if(ChildData = ctk::find(Data, "scale")) {
        Transform.Scale = load_vec3(ChildData);
    }
    return Transform;
}

static void error_callback(s32 Error, cstr Description) {
    CTK_FATAL("[%d] %s", Error, Description)
}

static mesh create_mesh(vulkan_instance* VulkanInstance, cstr Path) {
    mesh Mesh = {};

    vtk::device* Device = &VulkanInstance->Device;
    VkCommandPool GraphicsCommandPool = VulkanInstance->GraphicsCommandPool;
    vtk::buffer* DeviceBuffer = &VulkanInstance->DeviceBuffer;
    vtk::region* StagingRegion = &VulkanInstance->StagingRegion;

    u32 ProcessingFlags = aiProcess_CalcTangentSpace |
                          aiProcess_Triangulate |
                          aiProcess_JoinIdenticalVertices |
                          aiProcess_SortByPType;
    const aiScene* Scene = aiImportFile(Path, ProcessingFlags);
    if(Scene == NULL || Scene->mRootNode == NULL || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
        CTK_FATAL("error loading mesh from path \"%s\": %s", Path, aiGetErrorString())
    }

    ////////////////////////////////////////////////////////////
    /// Allocation
    ////////////////////////////////////////////////////////////
    ctk::todo("HACK: Baking all meshes from file into single mesh.");
    u32 VertexCount = 0;
    u32 IndexCount = 0;
    for(u32 MeshIndex = 0; MeshIndex < Scene->mNumMeshes; ++MeshIndex) {
        aiMesh* Mesh = Scene->mMeshes[MeshIndex];
        VertexCount += Mesh->mNumVertices;
        for(u32 FaceIndex = 0; FaceIndex < Mesh->mNumFaces; ++FaceIndex) {
            IndexCount += Mesh->mFaces[FaceIndex].mNumIndices;
        }
    }
    Mesh.Vertexes = ctk::create_array_empty<vertex>(VertexCount);
    Mesh.Indexes = ctk::create_array_empty<u32>(IndexCount);

    ////////////////////////////////////////////////////////////
    /// Processing
    ////////////////////////////////////////////////////////////
    for(u32 MeshIndex = 0; MeshIndex < Scene->mNumMeshes; ++MeshIndex) {
        aiMesh* SceneMesh = Scene->mMeshes[MeshIndex];
        u32 IndexBase = Mesh.Vertexes.Count;
        for(u32 VertexIndex = 0; VertexIndex < SceneMesh->mNumVertices; ++VertexIndex) {
            vertex* Vertex = ctk::push(&Mesh.Vertexes);
            aiVector3D* Position = SceneMesh->mVertices + VertexIndex;
            aiVector3D* Normal = SceneMesh->mNormals + VertexIndex;
            Vertex->Position = { Position->x, Position->y, Position->z };
            Vertex->Normal = { Normal->x, Normal->y, Normal->z };

            // Texture coordinates are optional.
            if(SceneMesh->mTextureCoords[0] == NULL) {
                Vertex->UV = { 0, 0 };
            } else {
                aiVector3D* UV = SceneMesh->mTextureCoords[0] + VertexIndex;
                Vertex->UV = { UV->x, 1 - UV->y }; // Blender's UV y-axis is inverse from Vulkan's.
            }
        }
        for(u32 FaceIndex = 0; FaceIndex < SceneMesh->mNumFaces; ++FaceIndex) {
            aiFace* Face = SceneMesh->mFaces + FaceIndex;
            for(u32 IndexIndex = 0; IndexIndex < Face->mNumIndices; ++IndexIndex) {
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

static void check_vk_result(VkResult Result) {
    vtk::validate_vk_result(Result, "<imgui_internal>", "imgui internal call failed");
}

////////////////////////////////////////////////////////////
/// Window
////////////////////////////////////////////////////////////
struct window {
    GLFWwindow* Handle;
    u32 Width;
    u32 Height;
};

struct WindowInfo {
    void *user_pointer;
    void (*key_callback)(GLFWwindow* Window, s32 Key, s32 Scancode, s32 Action, s32 Mods);
    void (*mouse_button_callback)(GLFWwindow* Window, s32 Button, s32 Action, s32 Mods);
};

static window *create_window(WindowInfo *info) {
    auto Window = ctk::allocate<window>();
    *Window = {};
    ctk::data Data = ctk::load_data("assets/data/window.ctkd");
    glfwSetErrorCallback(error_callback);
    if(glfwInit() != GLFW_TRUE) {
        CTK_FATAL("failed to init GLFW")
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    Window->Width = ctk::to_s32(&Data, "width");
    Window->Height = ctk::to_s32(&Data, "height");
    Window->Handle = glfwCreateWindow(Window->Width, Window->Height, ctk::to_cstr(&Data, "title"), NULL, NULL);
    if(Window->Handle == NULL) {
        CTK_FATAL("failed to create window")
    }
    glfwSetWindowPos(Window->Handle, ctk::to_s32(&Data, "x"), ctk::to_s32(&Data, "y"));
    glfwSetWindowUserPointer(Window->Handle, info->user_pointer);
    glfwSetKeyCallback(Window->Handle, info->key_callback);
    glfwSetMouseButtonCallback(Window->Handle, info->mouse_button_callback);
    return Window;
}

////////////////////////////////////////////////////////////
/// Interface
////////////////////////////////////////////////////////////
static vulkan_instance* create_vulkan_instance(window* Window) {
    auto VulkanInstance = ctk::allocate<vulkan_instance>();
    *VulkanInstance = {};

    vtk::instance* Instance = &VulkanInstance->Instance;
    VkSurfaceKHR* PlatformSurface = &VulkanInstance->PlatformSurface;
    vtk::device* Device = &VulkanInstance->Device;
    vtk::swapchain* Swapchain = &VulkanInstance->Swapchain;
    VkCommandPool* GraphicsCommandPool = &VulkanInstance->GraphicsCommandPool;
    vtk::buffer* HostBuffer = &VulkanInstance->HostBuffer;

    ctk::data VulkanInstanceData = ctk::load_data("assets/data/vulkan_instance.ctkd");

    // Instance
    u32 GLFWExtensionCount = 0;
    cstr* GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionCount);
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
    *Device = vtk::create_device(Instance->Handle,* PlatformSurface, &DeviceInfo);

    // Swapchain
    *Swapchain = vtk::create_swapchain(Device,* PlatformSurface);

    // Graphics Command Pool
    *GraphicsCommandPool = vtk::create_command_pool(Device->Logical, Device->QueueFamilyIndexes.Graphics);

    // Frame State
    VulkanInstance->FrameState = vtk::create_frame_state(Device->Logical, 2, Swapchain->Images.Count);

    // Buffers
    vtk::buffer_info HostBufferInfo = {};
    HostBufferInfo.Size = 256 * CTK_MEGABYTE;
    HostBufferInfo.UsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    HostBufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    *HostBuffer = vtk::create_buffer(Device, &HostBufferInfo);

    vtk::buffer_info DeviceBufferInfo = {};
    DeviceBufferInfo.Size = 256 * CTK_MEGABYTE;
    DeviceBufferInfo.UsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    DeviceBufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VulkanInstance->DeviceBuffer = vtk::create_buffer(Device, &DeviceBufferInfo);

    // Regions
    VulkanInstance->StagingRegion = vtk::allocate_region(HostBuffer, 64 * CTK_MEGABYTE);

    return VulkanInstance;
}

static assets* create_assets(vulkan_instance* VulkanInstance) {
    auto Assets = ctk::allocate<assets>();
    *Assets = {};

    vtk::device* Device = &VulkanInstance->Device;

    ctk::data AssetData = ctk::load_data("assets/data/assets.ctkd");

    ////////////////////////////////////////////////////////////
    /// Textures
    ////////////////////////////////////////////////////////////
    ctk::data* TextureMap = ctk::at(&AssetData, "textures");
    for(u32 TextureIndex = 0; TextureIndex < TextureMap->Children.Count; ++TextureIndex) {
        ctk::data* TextureData = ctk::at(TextureMap, TextureIndex);
        vtk::texture_info TextureInfo = {};
        TextureInfo.Filter = vtk::to_vk_filter(ctk::to_cstr(TextureData, "filter"));
        ctk::push(&Assets->Textures, TextureData->Key.Data,
                  vtk::create_texture(Device, VulkanInstance->GraphicsCommandPool, &VulkanInstance->StagingRegion,
                                      ctk::to_cstr(TextureData, "path"), &TextureInfo));
    }

    ////////////////////////////////////////////////////////////
    /// Shader Modules
    ////////////////////////////////////////////////////////////
    ctk::data* ShaderModuleMap = ctk::at(&AssetData, "shader_modules");
    for(u32 ShaderModuleIndex = 0; ShaderModuleIndex < ShaderModuleMap->Children.Count; ++ShaderModuleIndex) {
        ctk::data* ShaderModuleData = ctk::at(ShaderModuleMap, ShaderModuleIndex);
        VkShaderStageFlagBits Stage = vtk::to_vk_shader_stage_flag_bits(ctk::to_cstr(ShaderModuleData, "stage"));
        ctk::push(&Assets->ShaderModules, ShaderModuleData->Key.Data,
                  vtk::create_shader_module(Device->Logical, ctk::to_cstr(ShaderModuleData, "path"), Stage));
    }

    ////////////////////////////////////////////////////////////
    /// Meshes
    ////////////////////////////////////////////////////////////
    ctk::data* ModelMap = ctk::at(&AssetData, "models");
    for(u32 ModelIndex = 0; ModelIndex < ModelMap->Children.Count; ++ModelIndex) {
        ctk::data* ModelData = ctk::at(ModelMap, ModelIndex);
        ctk::push(&Assets->Meshes, ModelData->Key.Data, create_mesh(VulkanInstance, ctk::to_cstr(ModelData, "path")));
    }

    return Assets;
}

static void update_input_state(input_state* InputState, GLFWwindow* Window) {
    // Mouse Delta
    ctk::vec2 PreviousMousePosition = InputState->MousePosition;
    f64 CurrentMouseX = 0.0;
    f64 CurrentMouseY = 0.0;
    glfwGetCursorPos(Window, &CurrentMouseX, &CurrentMouseY);
    InputState->MousePosition = { CurrentMouseX, CurrentMouseY };

    // Calculate delta if previous position was not unset.
    if(PreviousMousePosition != UNSET_MOUSE_POSITION) {
        InputState->MouseDelta = InputState->MousePosition - PreviousMousePosition;
    }
}

static void camera_controls(transform* CameraTransform, input_state* InputState) {
    // Rotation
    if(InputState->MouseButtonDown[GLFW_MOUSE_BUTTON_2]) {
        static const f32 SENS = 0.4f;
        CameraTransform->Rotation.X += InputState->MouseDelta.Y * SENS;
        CameraTransform->Rotation.Y -= InputState->MouseDelta.X * SENS;
        CameraTransform->Rotation.X = ctk::clamp(CameraTransform->Rotation.X, -80.0f, 80.0f);
    }

    // Translation
    ctk::vec3<f32> Translation = {};
    f32 Modifier = InputState->KeyDown[GLFW_KEY_LEFT_SHIFT] ? 4 :
                   InputState->KeyDown[GLFW_KEY_LEFT_CONTROL] ? 1 :
                   2;
    if(InputState->KeyDown[GLFW_KEY_W]) Translation.Z += 0.01f * Modifier;
    if(InputState->KeyDown[GLFW_KEY_S]) Translation.Z -= 0.01f * Modifier;
    if(InputState->KeyDown[GLFW_KEY_D]) Translation.X += 0.01f * Modifier;
    if(InputState->KeyDown[GLFW_KEY_A]) Translation.X -= 0.01f * Modifier;
    if(InputState->KeyDown[GLFW_KEY_E]) Translation.Y -= 0.01f * Modifier;
    if(InputState->KeyDown[GLFW_KEY_Q]) Translation.Y += 0.01f * Modifier;
    local_translate(CameraTransform, Translation);
}

static u32 aquire_next_swapchain_image_index(vulkan_instance* VulkanInstance) {
    vtk::device* Device = &VulkanInstance->Device;
    vtk::frame_state* FrameState = &VulkanInstance->FrameState;
    vtk::frame* CurrentFrame = FrameState->Frames + FrameState->CurrentFrameIndex;

    // Wait on current frame's fence if still unsignaled.
    vkWaitForFences(Device->Logical, 1, &CurrentFrame->InFlightFence, VK_TRUE, UINT64_MAX);

    // Aquire next swapchain image index, using a semaphore to signal when image is available for rendering.
    u32 SwapchainImageIndex = VTK_UNSET_INDEX;
    vtk::validate_vk_result(vkAcquireNextImageKHR(Device->Logical, VulkanInstance->Swapchain.Handle, UINT64_MAX,
                                                  CurrentFrame->ImageAquiredSemaphore, VK_NULL_HANDLE, &SwapchainImageIndex),
                            "vkAcquireNextImageKHR", "failed to aquire next swapchain image");

    return SwapchainImageIndex;
}

static glm::mat4 view_projection_matrix(camera* Camera) {
    transform* CameraTransform = &Camera->Transform;

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
    glm::mat4 ProjectionMatrix = glm::perspective(glm::radians(Camera->FieldOfView), Camera->Aspect, Camera->ZNear, Camera->ZFar);
    ProjectionMatrix[1][1] *= -1; // Flip y value for scale (glm is designed for OpenGL).

    return ProjectionMatrix * ViewMatrix;
}

static void synchronize_current_frame(vulkan_instance* VulkanInstance, u32 SwapchainImageIndex) {
    vtk::device* Device = &VulkanInstance->Device;
    vtk::frame_state* FrameState = &VulkanInstance->FrameState;

    vtk::frame* CurrentFrame = FrameState->Frames + FrameState->CurrentFrameIndex;

    // Wait on swapchain image's previously associated frame fence before rendering.
    VkFence* PreviousFrameInFlightFence = FrameState->PreviousFrameInFlightFences + SwapchainImageIndex;
    if(*PreviousFrameInFlightFence != VK_NULL_HANDLE) {
        vkWaitForFences(Device->Logical, 1, PreviousFrameInFlightFence, VK_TRUE, UINT64_MAX);
    }
    vkResetFences(Device->Logical, 1, &CurrentFrame->InFlightFence);
    *PreviousFrameInFlightFence = CurrentFrame->InFlightFence;
}

static void submit_render_passes(vulkan_instance* VulkanInstance, vtk::render_pass** RenderPasses, u32 RenderPassCount, u32 SwapchainImageIndex) {
    vtk::device* Device = &VulkanInstance->Device;
    vtk::swapchain* Swapchain = &VulkanInstance->Swapchain;
    vtk::frame_state* FrameState = &VulkanInstance->FrameState;
    vtk::frame* CurrentFrame = FrameState->Frames + FrameState->CurrentFrameIndex;

    ////////////////////////////////////////////////////////////
    /// Command Buffers Submission
    ////////////////////////////////////////////////////////////
    VkSemaphore WaitSemaphores[] = { CurrentFrame->ImageAquiredSemaphore };
    VkPipelineStageFlags WaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore SignalSemaphores[] = { CurrentFrame->RenderFinishedSemaphore };
    ctk::sarray<VkCommandBuffer, 4> CommandBuffers = {};
    CTK_ITERATE(RenderPassCount) ctk::push(&CommandBuffers, RenderPasses[IterationIndex]->CommandBuffers[SwapchainImageIndex]);

    VkSubmitInfo SubmitInfos[1] = {};
    SubmitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfos[0].waitSemaphoreCount = CTK_ARRAY_COUNT(WaitSemaphores);
    SubmitInfos[0].pWaitSemaphores = WaitSemaphores;
    SubmitInfos[0].pWaitDstStageMask = WaitStages;
    SubmitInfos[0].commandBufferCount = CommandBuffers.Count;
    SubmitInfos[0].pCommandBuffers = CommandBuffers.Data;
    SubmitInfos[0].signalSemaphoreCount = CTK_ARRAY_COUNT(SignalSemaphores);
    SubmitInfos[0].pSignalSemaphores = SignalSemaphores;

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
}

static void cycle_frame(vulkan_instance* VulkanInstance) {
    vtk::frame_state* FrameState = &VulkanInstance->FrameState;
    FrameState->CurrentFrameIndex = (FrameState->CurrentFrameIndex + 1) % FrameState->Frames.Count;
}

static ui* create_ui(window* Window, vulkan_instance* VulkanInstance) {
    vtk::device* Device = &VulkanInstance->Device;
    vtk::swapchain* Swapchain = &VulkanInstance->Swapchain;

    ////////////////////////////////////////////////////////////
    /// GLFW Init
    ////////////////////////////////////////////////////////////
    auto UI = ctk::allocate<ui>();
    *UI = {};
    ImGui::CreateContext();
    UI->IO = &ImGui::GetIO();
    if (!ImGui_ImplGlfw_InitForVulkan(Window->Handle, true /* install cbs so imgui can handle glfw input */)) {
        CTK_FATAL("failed to init imgui glfw for vulkan")
    }

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    vtk::render_pass_info RenderPassInfo = {};

    // Attachments
    vtk::attachment* SwapchainAttachment = ctk::push(&RenderPassInfo.Attachments);
    SwapchainAttachment->Description.format = Swapchain->ImageFormat;
    SwapchainAttachment->Description.samples = VK_SAMPLE_COUNT_1_BIT;
    SwapchainAttachment->Description.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    SwapchainAttachment->Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    SwapchainAttachment->Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    SwapchainAttachment->Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    SwapchainAttachment->Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    SwapchainAttachment->Description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    SwapchainAttachment->ClearValue = { 0, 0, 0, 0 };

    // Subpasses
    vtk::subpass* Subpass = ctk::push(&RenderPassInfo.Subpasses);
    ctk::push(&Subpass->ColorAttachmentReferences, { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

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
    CTK_ITERATE(Swapchain->Images.Count) {
        vtk::framebuffer_info* FramebufferInfo = ctk::push(&RenderPassInfo.FramebufferInfos);
        ctk::push(&FramebufferInfo->Attachments, Swapchain->Images[IterationIndex].View);
        FramebufferInfo->Extent = Swapchain->Extent;
        FramebufferInfo->Layers = 1;
    }

    UI->RenderPass = vtk::create_render_pass(Device->Logical, VulkanInstance->GraphicsCommandPool, &RenderPassInfo);

    ////////////////////////////////////////////////////////////
    /// Descriptor Pool
    ////////////////////////////////////////////////////////////
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
    vtk::validate_vk_result(vkCreateDescriptorPool(Device->Logical, &DescriptorPoolCreateInfo, NULL, &UI->DescriptorPool),
                            "vkCreateDescriptorPool", "failed to create descriptor pool");

    ////////////////////////////////////////////////////////////
    /// Vulkan Init
    ////////////////////////////////////////////////////////////
    ImGui_ImplVulkan_InitInfo ImGuiVulkanInitInfo = {};
    ImGuiVulkanInitInfo.Instance = VulkanInstance->Instance.Handle;
    ImGuiVulkanInitInfo.PhysicalDevice = Device->Physical;
    ImGuiVulkanInitInfo.Device = Device->Logical;
    ImGuiVulkanInitInfo.QueueFamily = Device->QueueFamilyIndexes.Graphics;
    ImGuiVulkanInitInfo.Queue = Device->GraphicsQueue;
    ImGuiVulkanInitInfo.PipelineCache = VK_NULL_HANDLE;
    ImGuiVulkanInitInfo.DescriptorPool = UI->DescriptorPool;
    ImGuiVulkanInitInfo.MinImageCount = Swapchain->Images.Count;
    ImGuiVulkanInitInfo.ImageCount = Swapchain->Images.Count;
    ImGuiVulkanInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGuiVulkanInitInfo.Allocator = NULL;
    ImGuiVulkanInitInfo.CheckVkResultFn = check_vk_result;
    if(!ImGui_ImplVulkan_Init(&ImGuiVulkanInitInfo, UI->RenderPass.Handle)) {
        CTK_FATAL("failed to init imgui vulkan")
    }

    ////////////////////////////////////////////////////////////
    /// Fonts
    ////////////////////////////////////////////////////////////
    VkCommandBuffer CommandBuffer = vtk::begin_one_time_command_buffer(Device->Logical, VulkanInstance->GraphicsCommandPool);
        ImGui_ImplVulkan_CreateFontsTexture(CommandBuffer);
    vtk::submit_one_time_command_buffer(Device->Logical, Device->GraphicsQueue, VulkanInstance->GraphicsCommandPool, CommandBuffer);
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    return UI;
}

static void ui_new_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

static void record_ui_command_buffer(vulkan_instance* VulkanInstance, ui* UI, u32 SwapchainImageIndex) {
    vtk::render_pass* RenderPass = &UI->RenderPass;
    ImGui::Render();

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = 0;
    CommandBufferBeginInfo.pInheritanceInfo = NULL;

    VkCommandBuffer CommandBuffer = RenderPass->CommandBuffers[SwapchainImageIndex];
    vtk::validate_vk_result(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo), "vkBeginCommandBuffer", "failed to begin ui command buffer");

    VkRect2D RenderArea = {};
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent = VulkanInstance->Swapchain.Extent;

    VkRenderPassBeginInfo RenderPassBeginInfo = {};
    RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RenderPassBeginInfo.renderPass = RenderPass->Handle;
    RenderPassBeginInfo.framebuffer = RenderPass->Framebuffers[SwapchainImageIndex];
    RenderPassBeginInfo.renderArea = RenderArea;
    RenderPassBeginInfo.clearValueCount = RenderPass->ClearValues.Count;
    RenderPassBeginInfo.pClearValues = RenderPass->ClearValues.Data;

    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), CommandBuffer);
    vkCmdEndRenderPass(CommandBuffer);
    vtk::validate_vk_result(vkEndCommandBuffer(CommandBuffer), "vkEndCommandBuffer", "error during ui command recording");
}
