#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vtk/vtk.h"
#include "ctk/ctk.h"
#include "ctk/math.h"

////////////////////////////////////////////////////////////
/// Constants
////////////////////////////////////////////////////////////
static const ctk::vec2<f64> GFX_UNSET_MOUSE_POSITION = { -10000.0, -10000.0 };

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
    ctk::vec2<f64> MousePosition = GFX_UNSET_MOUSE_POSITION;
    ctk::vec2<f64> MouseDelta;
};

struct vulkan_instance
{
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

struct vertex
{
    ctk::vec3<f32> Position;
    ctk::vec3<f32> Normal;
    ctk::vec2<f32> UV;
};

struct mesh
{
    ctk::array<vertex> Vertexes;
    ctk::array<u32> Indexes;
    vtk::region VertexRegion;
    vtk::region IndexRegion;
};

struct assets
{
    ctk::smap<mesh, 16> Meshes;
    ctk::smap<vtk::texture, 16> Textures;
    ctk::smap<vtk::shader_module, 16> ShaderModules;
};

struct vulkan_state
{
    VkDescriptorPool DescriptorPool;
    vtk::vertex_layout VertexLayout;
    ctk::smap<vtk::uniform_buffer, 16> UniformBuffers;
    ctk::smap<vtk::descriptor_set, 16> DescriptorSets;
    ctk::smap<u32, 4> VertexAttributeIndexes;
    ctk::smap<vtk::graphics_pipeline, 16> GraphicsPipelines;
    ctk::smap<vtk::image, 4> Images;
    ctk::smap<vtk::render_pass, 4> RenderPasses;
};

struct entity_ubo
{
    alignas(16) glm::mat4 ModelMatrix;
    alignas(16) glm::mat4 ModelViewProjectionMatrix;
};

struct light_ubo
{
    alignas(16) ctk::vec4<f32> Color;
    alignas(16) ctk::vec3<f32> Position;
    f32 Linear;
    f32 Quadratic;
    f32 Cutoff;
    f32 OuterCutoff;
};

struct transform
{
    ctk::vec3<f32> Position;
    ctk::vec3<f32> Rotation;
    ctk::vec3<f32> Scale;
};

struct camera
{
    transform Transform;
    f32 FieldOfView;
};

struct entity
{
    transform Transform;
    ctk::sarray<vtk::descriptor_set *, 4> DescriptorSets;
    vtk::graphics_pipeline *GraphicsPipeline;
    mesh *Mesh;
};

struct scene
{
    camera Camera;
    ctk::smap<entity, 1024> Entities;
    ctk::sarray<entity_ubo, 1024> EntityUBOs;
    vtk::uniform_buffer *EntityUniformBuffer;
};

////////////////////////////////////////////////////////////
/// Interface
////////////////////////////////////////////////////////////
window *
create_window(input_state *InputState);

vulkan_instance *
create_vulkan_instance(window *Window);

assets *
create_assets(vulkan_instance *VulkanInstance);

vulkan_state *
create_vulkan_state(vulkan_instance *VulkanInstance, assets *Assets);

entity*
push_entity(scene* Scene, cstr Name);

scene *
create_scene(assets *Assets, vulkan_state *VulkanState, cstr Path);

void
record_default_command_buffers(vulkan_instance *VulkanInstance, vulkan_state *VulkanState, vtk::render_pass *RenderPass, scene *Scene);

u32
aquire_next_swapchain_image_index(vulkan_instance *VulkanInstance);

void
update_uniform_data(vulkan_instance *VulkanInstance, scene *Scene, u32 SwapchainImageIndex);

void
submit_render_pass(vulkan_instance *VulkanInstance, vulkan_state *VulkanState, vtk::render_pass *RenderPass, u32 SwapchainImageIndex);

void
local_translate(transform *Transform, ctk::vec3<f32> Translation);
