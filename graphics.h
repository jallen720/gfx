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
struct gfx_window
{
    GLFWwindow *Handle;
    u32 Width;
    u32 Height;
};

struct gfx_input_state
{
    b32 KeyDown[GLFW_KEY_LAST + 1];
    b32 MouseButtonDown[GLFW_MOUSE_BUTTON_LAST + 1];
    ctk::vec2<f64> MousePosition = GFX_UNSET_MOUSE_POSITION;
    ctk::vec2<f64> MouseDelta;
};

struct gfx_vulkan_instance
{
    vtk::instance Instance;
    VkSurfaceKHR PlatformSurface;
    vtk::device Device;
    vtk::swapchain Swapchain;
    VkCommandPool GraphicsCommandPool;
    vtk::frame_state FrameState;
    vtk::image DepthImage;
    vtk::render_pass RenderPass;
    vtk::buffer HostBuffer;
    vtk::buffer DeviceBuffer;
    vtk::region StagingRegion;
};

struct gfx_vertex
{
    ctk::vec3<f32> Position;
    ctk::vec3<f32> Normal;
    ctk::vec2<f32> UV;
};

struct gfx_mesh
{
    ctk::array<gfx_vertex> Vertexes;
    ctk::array<u32> Indexes;
    vtk::region VertexRegion;
    vtk::region IndexRegion;
};

struct gfx_assets
{
    ctk::smap<gfx_mesh, 16> Meshes;
    ctk::smap<vtk::texture, 16> Textures;
    ctk::smap<vtk::shader_module, 16> ShaderModules;
};

struct gfx_vulkan_state
{
    VkDescriptorPool DescriptorPool;
    vtk::vertex_layout VertexLayout;
    ctk::smap<vtk::uniform_buffer, 16> UniformBuffers;
    ctk::smap<vtk::descriptor_set, 16> DescriptorSets;
    ctk::smap<u32, 4> VertexAttributeIndexes;
    ctk::smap<vtk::graphics_pipeline, 16> GraphicsPipelines;
};

struct gfx_entity_ubo
{
    alignas(16) glm::mat4 ModelMatrix;
    alignas(16) glm::mat4 MVPMatrix;
};

////////////////////////////////////////////////////////////
/// Interface
////////////////////////////////////////////////////////////
gfx_window *
gfx_create_window(gfx_input_state *InputState);

gfx_vulkan_instance *
gfx_create_vulkan_instance(gfx_window *Window);

gfx_assets *
gfx_create_assets(gfx_vulkan_instance *VulkanInstance);

gfx_vulkan_state *
gfx_create_vulkan_state(gfx_vulkan_instance *VulkanInstance, gfx_assets *Assets);
