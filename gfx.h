#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vtk/vtk.h"
#include "ctk/ctk.h"
#include "ctk/math.h"

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

struct vulkan_instance
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
    ctk::sarray<vertex, 24> Vertexes;
    ctk::sarray<u32, 32> Indexes;
    vtk::region VertexRegion;
    vtk::region IndexRegion;
};

struct assets
{
    ctk::smap<mesh, 4> Meshes;
    ctk::smap<vtk::texture, 4> Textures;
    ctk::smap<vtk::shader_module, 16> ShaderModules;
};

struct vulkan_state
{
    VkDescriptorPool DescriptorPool;
    vtk::vertex_layout VertexLayout;
    ctk::smap<vtk::uniform_buffer, 4> UniformBuffers;
    ctk::smap<vtk::descriptor_set, 4> DescriptorSets;
    ctk::smap<u32, 4> VertexAttributeIndexes;
    ctk::smap<vtk::graphics_pipeline, 4> GraphicsPipelines;
};

struct entity_ubo
{
    alignas(16) glm::mat4 ModelMatrix;
    alignas(16) glm::mat4 MVPMatrix;
};

////////////////////////////////////////////////////////////
/// Interface
////////////////////////////////////////////////////////////
window *
CreateWindow_(input_state *InputState);

vulkan_instance *
CreateVulkanInstance(window *Window);

assets *
CreateAssets(vulkan_instance *VulkanInstance);

vulkan_state *
CreateVulkanState(vulkan_instance *VulkanInstance, assets *Assets);

} // gfx
