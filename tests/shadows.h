#include <windows.h>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb/stb_image.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "gfx/imgui/imgui.h"
#include "gfx/imgui/imgui_impl_glfw.h"
#include "gfx/imgui/imgui_impl_vulkan.h"

#include "ctk/ctk_new.h"
#include "vtk/vtk_new.h"

////////////////////////////////////////////////////////////
/// Window
////////////////////////////////////////////////////////////
static struct ctk_v2<f64> const UNSET_MOUSE_POSITION = { -10000.0, -10000.0 };

struct window {
    GLFWwindow *handle;
    u32 width;
    u32 height;
    bool key_down[GLFW_KEY_LAST + 1];
    bool mouse_button_down[GLFW_MOUSE_BUTTON_LAST + 1];
    struct ctk_v2<f64> mouse_position = UNSET_MOUSE_POSITION;
    struct ctk_v2<f64> mouse_delta;
};

static void error_callback(s32 err, cstr msg) {
    CTK_FATAL("[%d] %s", err, msg)
}

static void key_callback(GLFWwindow *glfw_win, s32 key, s32 scancode, s32 action, s32 mods) {
    auto window = (struct window *)glfwGetWindowUserPointer(glfw_win);
    window->key_down[key] = action == GLFW_PRESS || action == GLFW_REPEAT;
}

static void mouse_button_callback(GLFWwindow *glfw_win, s32 button, s32 action, s32 mods) {
    auto window = (struct window *)glfwGetWindowUserPointer(glfw_win);
    window->mouse_button_down[button] = action == GLFW_PRESS || action == GLFW_REPEAT;
}

static struct window *create_window() {
    auto win = ctk_zalloc<struct window>();
    glfwSetErrorCallback(error_callback);
    if (glfwInit() != GLFW_TRUE)
        CTK_FATAL("failed to init glfw")
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    win->width = 1600;
    win->height = 900;
    win->handle = glfwCreateWindow(win->width, win->height, "test", NULL, NULL);
    if (win->handle == NULL)
        CTK_FATAL("failed to create win")
    glfwSetWindowPos(win->handle, 320, 60);
    glfwSetWindowUserPointer(win->handle, win);
    glfwSetKeyCallback(win->handle, key_callback);
    glfwSetMouseButtonCallback(win->handle, mouse_button_callback);
    return win;
}

////////////////////////////////////////////////////////////
/// Vulkan Core
////////////////////////////////////////////////////////////
struct vk_core {
    struct vtk_instance instance;
    VkSurfaceKHR surface;
    struct vtk_device device;
    struct vtk_swapchain swapchain;
    VkCommandPool graphics_cmd_pool;
    struct {
        struct vtk_buffer device;
        struct vtk_buffer host;
    } buffers;
    struct vtk_region staging_region;
};

static void create_buffers(struct vk_core *vk) {
    struct vtk_buffer_info host_buf_info = {};
    host_buf_info.size = 256 * CTK_MEGABYTE;
    host_buf_info.usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    host_buf_info.memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    host_buf_info.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    vk->buffers.host = vtk_create_buffer(&vk->device, &host_buf_info);

    struct vtk_buffer_info device_buf_info = {};
    device_buf_info.size = 256 * CTK_MEGABYTE;
    device_buf_info.usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    device_buf_info.memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    device_buf_info.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    vk->buffers.device = vtk_create_buffer(&vk->device, &device_buf_info);
}

static struct vk_core *create_vk_core(struct window *window) {
    auto vk = ctk_zalloc<vk_core>();

    vk->instance = vtk_create_instance();
    vtk_validate_result(glfwCreateWindowSurface(vk->instance.handle, window->handle, NULL, &vk->surface), "failed to create glfw surface");

    VkPhysicalDeviceFeatures features = {};
    features.geometryShader = VK_TRUE;
    features.samplerAnisotropy = VK_TRUE;
    vk->device = vtk_create_device(vk->instance.handle, vk->surface, &features);

    vk->swapchain = vtk_create_swapchain(&vk->device, vk->surface);

    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_info.queueFamilyIndex = vk->device.queue_family_indexes.graphics;
    vtk_validate_result(vkCreateCommandPool(vk->device.logical, &cmd_pool_info, NULL, &vk->graphics_cmd_pool), "failed to create command pool");

    create_buffers(vk);
    vk->staging_region = vtk_allocate_region(&vk->buffers.host, 64 * CTK_MEGABYTE);

    return vk;
}

////////////////////////////////////////////////////////////
/// App
////////////////////////////////////////////////////////////
struct asset_load_info {
    cstr name;
    cstr path;
};

struct shader_load_info : public asset_load_info {
    VkShaderStageFlagBits stage;
};

struct texture_load_info : public asset_load_info {
    VkFilter filter;
};

struct vertex {
    struct ctk_v3<f32> position;
    struct ctk_v3<f32> normal;
    struct ctk_v2<f32> uv;
};

struct mesh {
    struct ctk_buffer<struct vertex> vertexes;
    struct ctk_buffer<u32> indexes;
    struct vtk_region vertex_region;
    struct vtk_region index_region;
};

struct model_ubo {
    alignas(16) glm::mat4 model_mtx;
    alignas(16) glm::mat4 mvp_mtx;
};

enum {
    LIGHT_MODE_DIRECTIONAL,
    LIGHT_MODE_POINT,
};

static struct ctk_v2<f32> const LIGHT_ATTENUATION_CONSTS[] = {
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

struct light_ubo {
    struct ctk_v4<f32> color;
    struct ctk_v3<f32> pos;
    s32 mode;
    struct ctk_v3<f32> dir;
    f32 far_clip;
    s32 depth_bias;
    s32 normal_bias;
    f32 linear;
    f32 quadratic;
    f32 ambient;
};

struct material_ubo {
    u32 shine_exponent;
};

static u32 const MAX_ENTITIES = 1024;
static u32 const MAX_LIGHTS = 16;
static u32 const MAX_MATERIALS = 16;
static u32 const SHADOW_MAP_SIZE = 4096;
// static u32 const SHADOW_MAP_SIZE = 8192;
static VkFormat const OMNI_SHADOW_MAP_FORMAT = VK_FORMAT_D32_SFLOAT;//VK_FORMAT_R32_SFLOAT; // Image will have color aspect but hold depth data.

struct app {
    struct vtk_vertex_layout vertex_layout;
    struct {
        struct vtk_uniform_buffer entity_model_ubos;
        struct vtk_uniform_buffer light_model_ubos;
        struct vtk_uniform_buffer light_ubos;
    } uniform_bufs;
    struct {
        struct vtk_image depth;
    } attachment_imgs;
    struct {
        struct vtk_texture directional;
        struct vtk_texture omni;
    } shadow_maps;
    struct {
        VkCommandBuffer one_time;
        struct ctk_array<VkCommandBuffer, 4> render;
    } cmd_bufs;
    struct {
        struct ctk_map<struct vtk_shader, 16> shaders;
        struct ctk_map<struct vtk_texture, 16> textures;
        struct ctk_map<struct mesh, 16> meshes;
    } assets;
    struct {
        VkDescriptorPool pool;
        struct {
            VkDescriptorSetLayout model_ubo;
            VkDescriptorSetLayout light_ubo;
            VkDescriptorSetLayout sampler;
        } set_layouts;
        struct {
            struct vtk_descriptor_set entity_model_ubo;
            struct vtk_descriptor_set light_model_ubo;
            struct vtk_descriptor_set light_ubo;
            struct ctk_map<struct vtk_descriptor_set, 16> textures;
            struct {
                struct vtk_descriptor_set directional;
                struct vtk_descriptor_set omni;
            } shadow_maps;
        } sets;
    } descriptors;
    struct {
        struct vtk_render_pass direct;
        struct vtk_render_pass shadow;
        struct vtk_render_pass fullscreen_texture;
    } render_passes;
    struct {
        struct vtk_graphics_pipeline perspective_shadow;
        struct vtk_graphics_pipeline direct;
        struct vtk_graphics_pipeline unlit;
        struct vtk_graphics_pipeline fullscreen_texture;
    } graphics_pipelines;
    struct {
        struct ctk_array<VkSemaphore, 4> img_aquired;
        struct ctk_array<u32, 4> img_prev_frame;
        struct ctk_array<VkSemaphore, 4> render_finished;
        struct ctk_array<VkFence, 4> in_flight;
        u32 curr_frame;
        u32 frame_count;
    } frame_sync;
};

static struct vtk_texture load_texture(struct vtk_texture_info *info, cstr path, struct app *app, struct vk_core *vk) {
    CTK_TODO("batch mem barriers")

    // Load image from path and write its data to staging region.
    s32 width = 0;
    s32 height = 0;
    s32 channel_count = 0;
    stbi_uc *data = stbi_load(path, &width, &height, &channel_count, STBI_rgb_alpha);
    if (data == NULL)
        CTK_FATAL("failed to load image from \"%s\"", path)
    vtk_write_to_host_region(vk->device.logical, data, width * height * STBI_rgb_alpha, &vk->staging_region, 0);
    stbi_image_free(data);

    // Create texture based on dimensions of loaded image.
    info->image.extent.width = width;
    info->image.extent.height = height;
    info->image.format = VK_FORMAT_R8G8B8A8_UNORM;
    info->view.format = VK_FORMAT_R8G8B8A8_UNORM;
    struct vtk_texture tex = vtk_create_texture(info, &vk->device);

    // Copy image data (now in staging region) to texture image memory, transitioning texture image layout with pipeline barriers as necessary.
    vtk_begin_one_time_command_buffer(app->cmd_bufs.one_time);
        VkImageMemoryBarrier pre_mem_barrier = {};
        pre_mem_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        pre_mem_barrier.srcAccessMask = 0;
        pre_mem_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        pre_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        pre_mem_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        pre_mem_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre_mem_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre_mem_barrier.image = tex.handle;
        pre_mem_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        pre_mem_barrier.subresourceRange.baseMipLevel = 0;
        pre_mem_barrier.subresourceRange.levelCount = 1;
        pre_mem_barrier.subresourceRange.baseArrayLayer = 0;
        pre_mem_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(app->cmd_bufs.one_time,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, // Dependency Flags
                             0, NULL, // Memory Barriers
                             0, NULL, // Buffer Memory Barriers
                             1, &pre_mem_barrier); // Image Memory Barriers

        VkBufferImageCopy copy = {};
        copy.bufferOffset = 0;
        copy.bufferRowLength = 0;
        copy.bufferImageHeight = 0;
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageOffset.x = 0;
        copy.imageOffset.y = 0;
        copy.imageOffset.z = 0;
        copy.imageExtent.width = width;
        copy.imageExtent.height = height;
        copy.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(app->cmd_bufs.one_time, vk->staging_region.buffer->handle, tex.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        VkImageMemoryBarrier post_mem_barrier = {};
        post_mem_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        post_mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        post_mem_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        post_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        post_mem_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        post_mem_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post_mem_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post_mem_barrier.image = tex.handle;
        post_mem_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        post_mem_barrier.subresourceRange.baseMipLevel = 0;
        post_mem_barrier.subresourceRange.levelCount = 1;
        post_mem_barrier.subresourceRange.baseArrayLayer = 0;
        post_mem_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(app->cmd_bufs.one_time,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, // Dependency Flags
                             0, NULL, // Memory Barriers
                             0, NULL, // Buffer Memory Barriers
                             1, &post_mem_barrier); // Image Memory Barriers
    vtk_submit_one_time_command_buffer(app->cmd_bufs.one_time, vk->device.queues.graphics);
    return tex;
}

static void load_mesh(struct mesh *mesh, cstr path, struct app *app, struct vk_core *vk) {
    u32 process_flags = aiProcess_CalcTangentSpace |
                        aiProcess_Triangulate |
                        aiProcess_JoinIdenticalVertices |
                        aiProcess_SortByPType;
    aiScene const *scene = aiImportFile(path, process_flags);
    if (scene == NULL || scene->mRootNode == NULL || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
        CTK_FATAL("error loading mesh from path \"%s\": %s", path, aiGetErrorString())

    // Allocation
    CTK_TODO("HACK: Baking all meshes from file into single mesh.")
    u32 vert_count = 0;
    u32 idx_count = 0;
    for (u32 mesh_idx = 0; mesh_idx < scene->mNumMeshes; ++mesh_idx) {
        aiMesh *scene_mesh = scene->mMeshes[mesh_idx];
        vert_count += scene_mesh->mNumVertices;
        for (u32 face_idx = 0; face_idx < scene_mesh->mNumFaces; ++face_idx)
            idx_count += scene_mesh->mFaces[face_idx].mNumIndices;
    }
    mesh->vertexes = ctk_create_buffer<struct vertex>(vert_count);
    mesh->indexes = ctk_create_buffer<u32>(idx_count);

    // Processing
    for (u32 mesh_idx = 0; mesh_idx < scene->mNumMeshes; ++mesh_idx) {
        aiMesh *scene_mesh = scene->mMeshes[mesh_idx];
        u32 idx_base = mesh->vertexes.count;
        for (u32 vert_idx = 0; vert_idx < scene_mesh->mNumVertices; ++vert_idx) {
            struct vertex *vert = ctk_push(&mesh->vertexes);
            aiVector3D *position = scene_mesh->mVertices + vert_idx;
            aiVector3D *normal = scene_mesh->mNormals + vert_idx;
            vert->position = { position->x, position->y, position->z };
            vert->normal = { normal->x, normal->y, normal->z };

            // Texture coordinates are optional.
            if (scene_mesh->mTextureCoords[0] == NULL) {
                vert->uv = { 0, 0 };
            } else {
                aiVector3D *uv = scene_mesh->mTextureCoords[0] + vert_idx;
                vert->uv = { uv->x, 1 - uv->y }; // Blender's uv y-axis is inverse from Vulkan's.
            }
        }
        for (u32 face_idx = 0; face_idx < scene_mesh->mNumFaces; ++face_idx) {
            aiFace *face = scene_mesh->mFaces + face_idx;
            for (u32 index_idx = 0; index_idx < face->mNumIndices; ++index_idx)
                ctk_push(&mesh->indexes, idx_base + face->mIndices[index_idx]);
        }
    }

    // Allocate and write vertex/index data to their associated regions.
    u32 verts_byte_size = ctk_byte_size(&mesh->vertexes);
    u32 idxs_byte_size = ctk_byte_size(&mesh->indexes);
    mesh->vertex_region = vtk_allocate_region(&vk->buffers.device, verts_byte_size, 4);
    mesh->index_region = vtk_allocate_region(&vk->buffers.device, idxs_byte_size, 4);
    vtk_begin_one_time_command_buffer(app->cmd_bufs.one_time);
        vtk_write_to_device_region(&vk->device, app->cmd_bufs.one_time, mesh->vertexes.data, verts_byte_size,
                                   &vk->staging_region, 0, &mesh->vertex_region, 0);
        vtk_write_to_device_region(&vk->device, app->cmd_bufs.one_time, mesh->indexes.data, idxs_byte_size,
                                   &vk->staging_region, verts_byte_size, &mesh->index_region, 0);
    vtk_submit_one_time_command_buffer(app->cmd_bufs.one_time, vk->device.queues.graphics);

    // Cleanup
    aiReleaseImport(scene);
}

static void load_assets(struct app *app, struct vk_core *vk) {
    // Shaders
    struct shader_load_info shader_load_infos[] = {
        { "shadow_vert", "assets/shaders/shadows/shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT },
        { "shadow_frag", "assets/shaders/shadows/shadow.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT },
        { "perspective_shadow_vert", "assets/shaders/shadows/perspective_shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT },
        { "perspective_shadow_frag", "assets/shaders/shadows/perspective_shadow.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT },
        { "direct_vert", "assets/shaders/shadows/direct.vert.spv", VK_SHADER_STAGE_VERTEX_BIT },
        { "direct_frag", "assets/shaders/shadows/direct.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT },
        { "unlit_vert", "assets/shaders/shadows/unlit.vert.spv", VK_SHADER_STAGE_VERTEX_BIT },
        { "unlit_frag", "assets/shaders/shadows/unlit.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT },
        { "fullscreen_texture_vert", "assets/shaders/shadows/fullscreen_texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT },
        { "fullscreen_texture_frag", "assets/shaders/shadows/fullscreen_texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT },
    };
    for (u32 i = 0; i < CTK_ARRAY_COUNT(shader_load_infos); ++i) {
        struct shader_load_info *shader_load_info = shader_load_infos + i;
        ctk_push(&app->assets.shaders, shader_load_info->name, vtk_create_shader(vk->device.logical, shader_load_info->path, shader_load_info->stage));
    }

    // Textures
    struct texture_load_info texture_load_infos[] = {
        { "wood", "assets/textures/wood.png", VK_FILTER_LINEAR },
        { "brick", "assets/textures/brick.jpeg", VK_FILTER_LINEAR },
    };
    for (u32 i = 0; i < CTK_ARRAY_COUNT(texture_load_infos); ++i) {
        struct texture_load_info *asset_load_info = texture_load_infos + i;
        struct vtk_texture_info info = vtk_default_texture_info();
        info.sampler.minFilter = asset_load_info->filter;
        info.sampler.magFilter = asset_load_info->filter;
        ctk_push(&app->assets.textures, asset_load_info->name, load_texture(&info, asset_load_info->path, app, vk));
    }

    // Meshes
    struct asset_load_info mesh_infos[] = {
        { "cube", "assets/models/cube.obj" },
        { "true_cube", "assets/models/true_cube.obj" },
        { "quad", "assets/models/quad.obj" },
        { "fullscreen_quad", "assets/models/fullscreen_quad.obj" },
        { "light_diamond", "assets/models/light_diamond.obj" },
        { "sibenik", "assets/models/sibenik/sibenik.obj" },
    };
    for (u32 i = 0; i < CTK_ARRAY_COUNT(mesh_infos); ++i) {
        struct asset_load_info *mesh_info = mesh_infos + i;
        load_mesh(ctk_push(&app->assets.meshes, mesh_info->name), mesh_info->path, app, vk);
    }
}

static void allocate_shadow_map_descriptor_set(struct app *app, struct vk_core *vk, struct vtk_descriptor_set *ds) {
    ds->instances.count = 1;

    VkDescriptorSetAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = app->descriptors.pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &app->descriptors.set_layouts.sampler;
    vtk_validate_result(vkAllocateDescriptorSets(vk->device.logical, &info, ds->instances.data), "failed to allocate descriptor sets");
}

static void create_descriptor_sets(struct app *app, struct vk_core *vk) {
    // Pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 16 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16 },
        // { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 16 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 },
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = 64;
    pool_info.poolSizeCount = CTK_ARRAY_COUNT(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    vtk_validate_result(vkCreateDescriptorPool(vk->device.logical, &pool_info, NULL, &app->descriptors.pool), "failed to create descriptor pool");

    // Layouts

    // model_ubo
    {
        VkDescriptorSetLayoutBinding binding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT };
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;
        vtk_validate_result(vkCreateDescriptorSetLayout(vk->device.logical, &info, NULL, &app->descriptors.set_layouts.model_ubo),
                            "error creating descriptor set layout");
    }

    // light_ubo
    {
        VkDescriptorSetLayoutBinding binding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;
        vtk_validate_result(vkCreateDescriptorSetLayout(vk->device.logical, &info, NULL, &app->descriptors.set_layouts.light_ubo),
                            "error creating descriptor set layout");
    }

    // sampler
    {
        VkDescriptorSetLayoutBinding binding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT };
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;
        vtk_validate_result(vkCreateDescriptorSetLayout(vk->device.logical, &info, NULL, &app->descriptors.set_layouts.sampler),
                            "error creating descriptor set layout");
    }

    // Sets

    // entity_model_ubo
    vtk_allocate_descriptor_set(&app->descriptors.sets.entity_model_ubo, app->descriptors.set_layouts.model_ubo, vk->swapchain.image_count, vk->device.logical,
                                app->descriptors.pool);
    ctk_push(&app->descriptors.sets.entity_model_ubo.dynamic_offsets, app->uniform_bufs.entity_model_ubos.element_size);

    // light_model_ubo
    vtk_allocate_descriptor_set(&app->descriptors.sets.light_model_ubo, app->descriptors.set_layouts.model_ubo, vk->swapchain.image_count, vk->device.logical,
                                app->descriptors.pool);
    ctk_push(&app->descriptors.sets.light_model_ubo.dynamic_offsets, app->uniform_bufs.light_model_ubos.element_size);

    // light_ubo
    vtk_allocate_descriptor_set(&app->descriptors.sets.light_ubo, app->descriptors.set_layouts.light_ubo, vk->swapchain.image_count, vk->device.logical,
                                app->descriptors.pool);
    ctk_push(&app->descriptors.sets.light_ubo.dynamic_offsets, app->uniform_bufs.light_ubos.element_size);

    // textures
    for (u32 i = 0; i < app->assets.textures.count; ++i) {
        struct vtk_descriptor_set *ds = ctk_push(&app->descriptors.sets.textures, app->assets.textures.keys[i]);
        ds->instances.count = 1;

        VkDescriptorSetAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = app->descriptors.pool;
        info.descriptorSetCount = ds->instances.count;
        info.pSetLayouts = &app->descriptors.set_layouts.sampler;
        vtk_validate_result(vkAllocateDescriptorSets(vk->device.logical, &info, ds->instances.data), "failed to allocate descriptor sets");
    }

    // shadow_maps
    allocate_shadow_map_descriptor_set(app, vk, &app->descriptors.sets.shadow_maps.directional);
    allocate_shadow_map_descriptor_set(app, vk, &app->descriptors.sets.shadow_maps.omni);

    // Updates
    struct ctk_array<VkDescriptorBufferInfo, 32> buf_infos = {};
    struct ctk_array<VkDescriptorImageInfo, 32> img_infos = {};
    struct ctk_array<VkWriteDescriptorSet, 32> writes = {};

    // entity_model_ubo
    for (u32 i = 0; i < app->uniform_bufs.entity_model_ubos.regions.count; ++i) {
        struct vtk_region *region = app->uniform_bufs.entity_model_ubos.regions + i;
        VkDescriptorBufferInfo *info = ctk_push(&buf_infos);
        info->buffer = region->buffer->handle;
        info->offset = region->offset;
        info->range = region->size;

        VkWriteDescriptorSet *write = ctk_push(&writes);
        write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write->dstSet = app->descriptors.sets.entity_model_ubo.instances[i];
        write->dstBinding = 0;
        write->dstArrayElement = 0;
        write->descriptorCount = 1;
        write->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        write->pBufferInfo = info;
    }

    // light_model_ubo
    for (u32 i = 0; i < app->uniform_bufs.light_model_ubos.regions.count; ++i) {
        struct vtk_region *region = app->uniform_bufs.light_model_ubos.regions + i;
        VkDescriptorBufferInfo *info = ctk_push(&buf_infos);
        info->buffer = region->buffer->handle;
        info->offset = region->offset;
        info->range = region->size;

        VkWriteDescriptorSet *write = ctk_push(&writes);
        write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write->dstSet = app->descriptors.sets.light_model_ubo.instances[i];
        write->dstBinding = 0;
        write->dstArrayElement = 0;
        write->descriptorCount = 1;
        write->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        write->pBufferInfo = info;
    }

    // light_ubo
    for (u32 i = 0; i < app->uniform_bufs.light_ubos.regions.count; ++i) {
        struct vtk_region *region = app->uniform_bufs.light_ubos.regions + i;
        VkDescriptorBufferInfo *info = ctk_push(&buf_infos);
        info->buffer = region->buffer->handle;
        info->offset = region->offset;
        info->range = region->size;

        VkWriteDescriptorSet *write = ctk_push(&writes);
        write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write->dstSet = app->descriptors.sets.light_ubo.instances[i];
        write->dstBinding = 0;
        write->dstArrayElement = 0;
        write->descriptorCount = 1;
        write->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        write->pBufferInfo = info;
    }

    // textures
    for (u32 i = 0; i < app->assets.textures.count; ++i) {
        struct vtk_texture *t = app->assets.textures.values + i;

        VkDescriptorImageInfo *info = ctk_push(&img_infos);
        info->sampler = t->sampler;
        info->imageView = t->view;
        info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        struct vtk_descriptor_set *ds = ctk_at(&app->descriptors.sets.textures, app->assets.textures.keys[i]);
        VkWriteDescriptorSet *write = ctk_push(&writes);
        write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write->dstSet = ds->instances[0];
        write->dstBinding = 0;
        write->dstArrayElement = 0;
        write->descriptorCount = 1;
        write->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write->pImageInfo = info;
    }

    // shadow_maps

    // directional
    {
        struct vtk_texture *shadow_map = &app->shadow_maps.directional;

        VkDescriptorImageInfo *info = ctk_push(&img_infos);
        info->sampler = shadow_map->sampler;
        info->imageView = shadow_map->view;
        info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        struct vtk_descriptor_set *ds = &app->descriptors.sets.shadow_maps.directional;
        VkWriteDescriptorSet *write = ctk_push(&writes);
        write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write->dstSet = ds->instances[0];
        write->dstBinding = 0;
        write->dstArrayElement = 0;
        write->descriptorCount = 1;
        write->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write->pImageInfo = info;
    }

    // omni
    {
        struct vtk_texture *shadow_map = &app->shadow_maps.omni;

        VkDescriptorImageInfo *info = ctk_push(&img_infos);
        info->sampler = shadow_map->sampler;
        info->imageView = shadow_map->view;
        info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        struct vtk_descriptor_set *ds = &app->descriptors.sets.shadow_maps.omni;
        VkWriteDescriptorSet *write = ctk_push(&writes);
        write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write->dstSet = ds->instances[0];
        write->dstBinding = 0;
        write->dstArrayElement = 0;
        write->descriptorCount = 1;
        write->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write->pImageInfo = info;
    }

    vkUpdateDescriptorSets(vk->device.logical, writes.count, writes.data, 0, NULL);
}

static void create_render_passes(struct app *app, struct vk_core *vk) {
    // Shadow
    {
        struct vtk_render_pass_info rp_info = {};

        // Attachment Descriptions
        VkAttachmentDescription *depth_attachment = ctk_push(&rp_info.attachment_descriptions);
        depth_attachment->format = VK_FORMAT_D32_SFLOAT;
        depth_attachment->samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment->finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ctk_push(&rp_info.clear_values, { 1.0f, 0 });

        // Subpass Infos
        struct vtk_subpass_info *main = ctk_push(&rp_info.subpass_infos);
        ctk_set(&main->depth_attachment_ref, { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });

        // Subpass Dependencies
        rp_info.subpass_dependencies.count = 1;

        // Synchronize depth output.
        rp_info.subpass_dependencies[0].srcSubpass = 0;
        rp_info.subpass_dependencies[0].dstSubpass = VK_SUBPASS_EXTERNAL;
        rp_info.subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        rp_info.subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        rp_info.subpass_dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ;
        rp_info.subpass_dependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        rp_info.subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Framebuffer Infos
        for (u32 i = 0; i < vk->swapchain.image_count; ++i) {
            struct vtk_framebuffer_info *fb_info = ctk_push(&rp_info.framebuffer_infos);
            ctk_push(&fb_info->attachments, app->shadow_maps.directional.view);
            fb_info->extent.width = SHADOW_MAP_SIZE;
            fb_info->extent.height = SHADOW_MAP_SIZE;
            fb_info->layers = 1;
        }

        app->render_passes.shadow = vtk_create_render_pass(vk->device.logical, vk->graphics_cmd_pool, &rp_info);
    }

    // Direct
    {
        struct vtk_render_pass_info rp_info = {};

        // Attachment Descriptions
        VkAttachmentDescription *depth_attachment = ctk_push(&rp_info.attachment_descriptions);
        depth_attachment->format = vk->device.depth_image_format;
        depth_attachment->samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment->finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        ctk_push(&rp_info.clear_values, { 1.0f, 0 });

        VkAttachmentDescription *swapchain_attachment = ctk_push(&rp_info.attachment_descriptions);
        swapchain_attachment->format = vk->swapchain.image_format;
        swapchain_attachment->samples = VK_SAMPLE_COUNT_1_BIT;
        swapchain_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        swapchain_attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        swapchain_attachment->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        swapchain_attachment->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        swapchain_attachment->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchain_attachment->finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ctk_push(&rp_info.clear_values, { 0, 0, 0, 1 });

        // Subpass Infos
        struct vtk_subpass_info *main = ctk_push(&rp_info.subpass_infos);
        ctk_set(&main->depth_attachment_ref, { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
        ctk_push(&main->color_attachment_refs, { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

        // Subpass Dependencies
        rp_info.subpass_dependencies.count = 0;
        // rp_info.subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        // rp_info.subpass_dependencies[0].dstSubpass = 0;
        // rp_info.subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        // rp_info.subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        // rp_info.subpass_dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        // rp_info.subpass_dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        // rp_info.subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Framebuffer Infos
        for (u32 i = 0; i < vk->swapchain.image_count; ++i) {
            struct vtk_framebuffer_info *fb_info = ctk_push(&rp_info.framebuffer_infos);
            ctk_push(&fb_info->attachments, app->attachment_imgs.depth.view);
            ctk_push(&fb_info->attachments, vk->swapchain.image_views[i]);
            fb_info->extent = vk->swapchain.extent;
            fb_info->layers = 1;
        }

        app->render_passes.direct = vtk_create_render_pass(vk->device.logical, vk->graphics_cmd_pool, &rp_info);
    }

    // Fullscreen Texture
    {
        struct vtk_render_pass_info rp_info = {};

        // Attachment Descriptions
        VkAttachmentDescription *swapchain_attachment = ctk_push(&rp_info.attachment_descriptions);
        swapchain_attachment->format = vk->swapchain.image_format;
        swapchain_attachment->samples = VK_SAMPLE_COUNT_1_BIT;
        swapchain_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        swapchain_attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        swapchain_attachment->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        swapchain_attachment->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        swapchain_attachment->initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchain_attachment->finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        ctk_push(&rp_info.clear_values, { 0, 0, 0, 1 });

        // Subpass Infos
        struct vtk_subpass_info *main = ctk_push(&rp_info.subpass_infos);
        ctk_push(&main->color_attachment_refs, { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

        // Subpass Dependencies
        rp_info.subpass_dependencies.count = 0;

        // Framebuffer Infos
        for (u32 i = 0; i < vk->swapchain.image_count; ++i) {
            struct vtk_framebuffer_info *fb_info = ctk_push(&rp_info.framebuffer_infos);
            ctk_push(&fb_info->attachments, vk->swapchain.image_views[i]);
            fb_info->extent = vk->swapchain.extent;
            fb_info->layers = 1;
        }

        app->render_passes.fullscreen_texture = vtk_create_render_pass(vk->device.logical, vk->graphics_cmd_pool, &rp_info);
    }
}

static void create_graphics_pipelines(struct app *app, struct vk_core *vk) {
    // Perspective Shadow
    {
        struct vtk_graphics_pipeline_info info = vtk_default_graphics_pipeline_info();
        ctk_push(&info.shaders, ctk_at(&app->assets.shaders, "perspective_shadow_vert"));
        ctk_push(&info.shaders, ctk_at(&app->assets.shaders, "perspective_shadow_frag"));
        ctk_push(&info.descriptor_set_layouts, app->descriptors.set_layouts.model_ubo);
        ctk_push(&info.push_constant_ranges, { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 80 });
        ctk_push(&info.vertex_inputs, { 0, 0, ctk_at(&app->vertex_layout.attributes, "position") });
        ctk_push(&info.vertex_input_binding_descriptions, { 0, app->vertex_layout.size, VK_VERTEX_INPUT_RATE_VERTEX });
        ctk_push(&info.viewports, { 0, 0, (f32)SHADOW_MAP_SIZE, (f32)SHADOW_MAP_SIZE, 0, 1 });
        ctk_push(&info.scissors, { 0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE });
        ctk_push(&info.color_blend_attachment_states, vtk_default_color_blend_attachment_state());
        info.depth_stencil_state.depthTestEnable = VK_TRUE;
        info.depth_stencil_state.depthWriteEnable = VK_TRUE;
        info.depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        app->graphics_pipelines.perspective_shadow = vtk_create_graphics_pipeline(vk->device.logical, &app->render_passes.shadow, 0, &info);
    }

    // Direct
    {
        struct vtk_graphics_pipeline_info info = vtk_default_graphics_pipeline_info();
        ctk_push(&info.shaders, ctk_at(&app->assets.shaders, "direct_vert"));
        ctk_push(&info.shaders, ctk_at(&app->assets.shaders, "direct_frag"));
        ctk_push(&info.descriptor_set_layouts, app->descriptors.set_layouts.light_ubo);
        ctk_push(&info.descriptor_set_layouts, app->descriptors.set_layouts.model_ubo);
        ctk_push(&info.descriptor_set_layouts, app->descriptors.set_layouts.sampler);
        ctk_push(&info.descriptor_set_layouts, app->descriptors.set_layouts.sampler);
        ctk_push(&info.descriptor_set_layouts, app->descriptors.set_layouts.sampler);
        ctk_push(&info.push_constant_ranges, { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(struct ctk_v3<f32>) });
        ctk_push(&info.vertex_inputs, { 0, 0, ctk_at(&app->vertex_layout.attributes, "position") });
        ctk_push(&info.vertex_inputs, { 0, 1, ctk_at(&app->vertex_layout.attributes, "normal") });
        ctk_push(&info.vertex_inputs, { 0, 2, ctk_at(&app->vertex_layout.attributes, "uv") });
        ctk_push(&info.vertex_input_binding_descriptions, { 0, app->vertex_layout.size, VK_VERTEX_INPUT_RATE_VERTEX });
        ctk_push(&info.viewports, { 0, 0, (f32)vk->swapchain.extent.width, (f32)vk->swapchain.extent.height, 0, 1 });
        ctk_push(&info.scissors, { 0, 0, vk->swapchain.extent.width, vk->swapchain.extent.height });
        ctk_push(&info.color_blend_attachment_states, vtk_default_color_blend_attachment_state());
        info.depth_stencil_state.depthTestEnable = VK_TRUE;
        info.depth_stencil_state.depthWriteEnable = VK_TRUE;
        info.depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        app->graphics_pipelines.direct = vtk_create_graphics_pipeline(vk->device.logical, &app->render_passes.direct, 0, &info);
    }

    // Unlit
    {
        struct vtk_graphics_pipeline_info info = vtk_default_graphics_pipeline_info();
        ctk_push(&info.shaders, ctk_at(&app->assets.shaders, "unlit_vert"));
        ctk_push(&info.shaders, ctk_at(&app->assets.shaders, "unlit_frag"));
        ctk_push(&info.descriptor_set_layouts, app->descriptors.set_layouts.light_ubo);
        ctk_push(&info.descriptor_set_layouts, app->descriptors.set_layouts.model_ubo);
        ctk_push(&info.push_constant_ranges, { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(struct ctk_v4<f32>) });
        ctk_push(&info.vertex_inputs, { 0, 0, ctk_at(&app->vertex_layout.attributes, "position") });
        ctk_push(&info.vertex_input_binding_descriptions, { 0, app->vertex_layout.size, VK_VERTEX_INPUT_RATE_VERTEX });
        ctk_push(&info.viewports, { 0, 0, (f32)vk->swapchain.extent.width, (f32)vk->swapchain.extent.height, 0, 1 });
        ctk_push(&info.scissors, { 0, 0, vk->swapchain.extent.width, vk->swapchain.extent.height });
        ctk_push(&info.color_blend_attachment_states, vtk_default_color_blend_attachment_state());
        info.depth_stencil_state.depthTestEnable = VK_TRUE;
        info.depth_stencil_state.depthWriteEnable = VK_TRUE;
        info.depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        app->graphics_pipelines.unlit = vtk_create_graphics_pipeline(vk->device.logical, &app->render_passes.direct, 0, &info);
    }

    // Fullscreen Texture
    {
        struct vtk_graphics_pipeline_info info = vtk_default_graphics_pipeline_info();
        ctk_push(&info.shaders, ctk_at(&app->assets.shaders, "fullscreen_texture_vert"));
        ctk_push(&info.shaders, ctk_at(&app->assets.shaders, "fullscreen_texture_frag"));
        ctk_push(&info.descriptor_set_layouts, app->descriptors.set_layouts.sampler);
        ctk_push(&info.vertex_inputs, { 0, 0, ctk_at(&app->vertex_layout.attributes, "position") });
        ctk_push(&info.vertex_inputs, { 0, 1, ctk_at(&app->vertex_layout.attributes, "uv") });
        ctk_push(&info.vertex_input_binding_descriptions, { 0, app->vertex_layout.size, VK_VERTEX_INPUT_RATE_VERTEX });
        static u32 const SIZE = 800;
        ctk_push(&info.viewports, { 1600 - (SIZE + 10), 10, SIZE, SIZE, 0, 1 });
        ctk_push(&info.scissors, { 1600 - (SIZE + 10), 10, SIZE, SIZE });
        ctk_push(&info.color_blend_attachment_states, vtk_default_color_blend_attachment_state());
        app->graphics_pipelines.fullscreen_texture = vtk_create_graphics_pipeline(vk->device.logical, &app->render_passes.fullscreen_texture, 0, &info);
    }
}

static void init_frame_sync(struct app *app, struct vk_core *vk) {
    app->frame_sync.frame_count = vk->swapchain.image_count;
    CTK_ASSERT(app->frame_sync.frame_count <= ctk_size(&app->frame_sync.img_aquired))
    app->frame_sync.img_aquired.count = app->frame_sync.frame_count;
    app->frame_sync.img_prev_frame.count = vk->swapchain.image_count;
    app->frame_sync.render_finished.count = app->frame_sync.frame_count;
    app->frame_sync.in_flight.count = app->frame_sync.frame_count;

    // Frame Sync Objects
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.flags = 0;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (u32 i = 0; i < app->frame_sync.frame_count; ++i) {
        vtk_validate_result(vkCreateSemaphore(vk->device.logical, &semaphore_info, NULL, app->frame_sync.img_aquired + i), "failed to create semaphore");
        vtk_validate_result(vkCreateSemaphore(vk->device.logical, &semaphore_info, NULL, app->frame_sync.render_finished + i), "failed to create semaphore");
        vtk_validate_result(vkCreateFence(vk->device.logical, &fence_info, NULL, app->frame_sync.in_flight + i), "failed to create fence");
    }

    for (u32 i = 0; i < vk->swapchain.image_count; ++i)
        app->frame_sync.img_prev_frame[i] = CTK_U32_MAX;
}

static void create_shadow_maps(struct app *app, struct vk_core *vk) {
    // Directional
    {
        struct vtk_texture_info info = vtk_default_texture_info();
        info.memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        info.image.extent.width = SHADOW_MAP_SIZE;
        info.image.extent.height = SHADOW_MAP_SIZE;
        info.image.format = VK_FORMAT_D32_SFLOAT;
        info.image.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        info.view.format = VK_FORMAT_D32_SFLOAT;
        info.view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        info.sampler.magFilter = VK_FILTER_NEAREST;
        info.sampler.minFilter = VK_FILTER_NEAREST;
        info.sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        app->shadow_maps.directional = vtk_create_texture(&info, &vk->device);
    }

    // Omni
    {
        struct vtk_texture_info info = {};
        info.memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        info.image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        info.image.imageType = VK_IMAGE_TYPE_2D;
        info.image.format = OMNI_SHADOW_MAP_FORMAT;
        info.image.extent.width = SHADOW_MAP_SIZE;
        info.image.extent.height = SHADOW_MAP_SIZE;
        info.image.extent.depth = 1;
        info.image.mipLevels = 1;
        info.image.arrayLayers = 6;
        info.image.samples = VK_SAMPLE_COUNT_1_BIT;
        info.image.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        info.image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.image.queueFamilyIndexCount = 0; // Ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT.
        info.image.pQueueFamilyIndices = NULL; // Ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT.
        info.image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        info.view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.view.flags = 0;
        info.view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        info.view.format = OMNI_SHADOW_MAP_FORMAT;
        info.view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        info.view.subresourceRange.baseMipLevel = 0;
        info.view.subresourceRange.levelCount = 1;
        info.view.subresourceRange.baseArrayLayer = 0;
        info.view.subresourceRange.layerCount = 6;

        info.sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.sampler.magFilter = VK_FILTER_LINEAR;
        info.sampler.minFilter = VK_FILTER_LINEAR;
        info.sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.sampler.anisotropyEnable = VK_TRUE;
        info.sampler.maxAnisotropy = 16;
        info.sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        info.sampler.unnormalizedCoordinates = VK_FALSE;
        info.sampler.compareEnable = VK_FALSE;
        info.sampler.compareOp = VK_COMPARE_OP_NEVER;
        info.sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.sampler.mipLodBias = 0.0f;
        info.sampler.minLod = 0.0f;
        info.sampler.maxLod = 0.0f;

        app->shadow_maps.omni = vtk_create_texture(&info, &vk->device);

        // Manually transition omni shadow-map layout as it is not implicitly transitioned by any subpasses.
        vtk_begin_one_time_command_buffer(app->cmd_bufs.one_time);
            VkImageMemoryBarrier img_barrier = {};
            img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            img_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            img_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            img_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            img_barrier.image = app->shadow_maps.omni.handle;
            img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            img_barrier.subresourceRange.baseMipLevel = 0;
            img_barrier.subresourceRange.levelCount = 1;
            img_barrier.subresourceRange.baseArrayLayer = 0;
            img_barrier.subresourceRange.layerCount = 6;
            vkCmdPipelineBarrier(app->cmd_bufs.one_time,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 0, // Dependency Flags
                                 0, NULL, // Memory Barriers
                                 0, NULL, // Buffer Memory Barriers
                                 1, &img_barrier); // Image Memory Barriers
        vtk_submit_one_time_command_buffer(app->cmd_bufs.one_time, vk->device.queues.graphics);
    }
}

static struct app *create_app(struct vk_core *vk) {
    auto app = ctk_zalloc<struct app>();

    // Vertex Layout
    vtk_push_vertex_attribute(&app->vertex_layout, "position", 3);
    vtk_push_vertex_attribute(&app->vertex_layout, "normal", 3);
    vtk_push_vertex_attribute(&app->vertex_layout, "uv", 2);

    // Uniform Buffers
    app->uniform_bufs.entity_model_ubos = vtk_create_uniform_buffer(&vk->buffers.host, &vk->device, MAX_ENTITIES,
                                                                    sizeof(struct model_ubo), vk->swapchain.image_count);
    app->uniform_bufs.light_model_ubos = vtk_create_uniform_buffer(&vk->buffers.host, &vk->device, MAX_LIGHTS,
                                                                   sizeof(struct model_ubo), vk->swapchain.image_count);
    app->uniform_bufs.light_ubos = vtk_create_uniform_buffer(&vk->buffers.host, &vk->device, MAX_LIGHTS,
                                                             sizeof(struct light_ubo), vk->swapchain.image_count);

    // Attachment Images
    struct vtk_image_info depth_image_info = vtk_default_image_info();
    depth_image_info.memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    depth_image_info.image.extent.width = vk->swapchain.extent.width;
    depth_image_info.image.extent.height = vk->swapchain.extent.height;
    depth_image_info.image.format = vk->device.depth_image_format;
    depth_image_info.image.tiling = VK_IMAGE_TILING_OPTIMAL;
    depth_image_info.image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_image_info.view.format = vk->device.depth_image_format;
    depth_image_info.view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    app->attachment_imgs.depth = vtk_create_image(&depth_image_info, &vk->device);

    // Command Buffers
    app->cmd_bufs.one_time = vtk_allocate_command_buffer(vk->device.logical, vk->graphics_cmd_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    app->cmd_bufs.render.count = vk->swapchain.image_count;
    vtk_allocate_command_buffers(vk->device.logical, vk->graphics_cmd_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, app->cmd_bufs.render.count,
                                 app->cmd_bufs.render.data);

    create_shadow_maps(app, vk);
    load_assets(app, vk);
    create_descriptor_sets(app, vk);
    create_render_passes(app, vk);
    create_graphics_pipelines(app, vk);
    init_frame_sync(app, vk);

    return app;
}

////////////////////////////////////////////////////////////
/// Scene
////////////////////////////////////////////////////////////
struct transform {
    struct ctk_v3<f32> position;
    struct ctk_v3<f32> rotation;
    struct ctk_v3<f32> scale;
};

struct camera {
    struct transform transform;
    f32 fov;
    f32 aspect;
    f32 z_near;
    f32 z_far;
};

struct entity {
    cstr name;
    struct transform *transform;
    struct mesh *mesh;
    struct vtk_descriptor_set *texture_desc_set;
};

struct light {
    struct transform *transform;
    struct model_ubo *model_ubo;
    struct light_ubo *ubo;
    u32 attenuation_index;
};

struct material {
    cstr name;
    struct material_ubo *ubo;
};

struct scene {
    struct camera camera;
    struct ctk_array<struct entity, MAX_ENTITIES> entities;
    struct ctk_array<struct light, MAX_LIGHTS> lights;
    struct ctk_array<struct material, MAX_MATERIALS> materials;
    struct {
        struct ctk_array<struct transform, MAX_ENTITIES> transforms;
        struct ctk_array<struct model_ubo, MAX_ENTITIES> model_ubos;
    } entity;
    struct {
        struct ctk_array<struct transform, MAX_LIGHTS> transforms;
        struct ctk_array<struct model_ubo, MAX_LIGHTS> model_ubos;
        struct ctk_array<struct light_ubo, MAX_LIGHTS> ubos;
    } light;
    struct {
        struct ctk_array<struct material_ubo, MAX_MATERIALS> ubos;
    } material;
};

static struct transform DEFAULT_TRANSFORM = { {}, {}, { 1, 1, 1 } };

static struct entity *push_entity(struct scene *s, cstr name = NULL) {
    if (s->entities.count == MAX_ENTITIES)
        CTK_FATAL("cannot push more entities to scene (max: %u)", MAX_ENTITIES)
    ctk_push(&s->entity.model_ubos);
    struct entity *e = ctk_push(&s->entities);
    e->transform = ctk_push(&s->entity.transforms, DEFAULT_TRANSFORM);
    return e;
}

static struct light *push_light(struct scene *s) {
    if (s->lights.count == MAX_LIGHTS)
        CTK_FATAL("cannot push more lights to scene (max: %u)", MAX_LIGHTS)
    struct light *l = ctk_push(&s->lights);
    l->transform = ctk_push(&s->light.transforms, DEFAULT_TRANSFORM);
    l->model_ubo = ctk_push(&s->light.model_ubos);
    l->ubo = ctk_push(&s->light.ubos);
    l->ubo->mode = LIGHT_MODE_POINT;
    l->ubo->color = { 1, 1, 1, 1 };
    l->ubo->normal_bias = 16;
    l->ubo->ambient = 0.3f;
    l->attenuation_index = 3;
    return l;
}

static struct material *push_material(struct scene *s, cstr name = NULL) {
    if (s->materials.count == MAX_MATERIALS)
        CTK_FATAL("cannot push more lights to scene (max: %u)", MAX_LIGHTS)
    struct material *m = ctk_push(&s->materials);
    m->name = name;
    m->ubo = ctk_push(&s->material.ubos);
    m->ubo->shine_exponent = 2;
    return m;
}

static struct scene *create_scene(struct app *app, struct vk_core *vk) {
    auto scene = ctk_zalloc<struct scene>();

    scene->camera.transform = DEFAULT_TRANSFORM;
    scene->camera.transform.position = { 10, -5, 14};
    scene->camera.transform.rotation = { 45, 0, 0 };
    scene->camera.fov = 90.0f;
    scene->camera.aspect = vk->swapchain.extent.width / (f32)vk->swapchain.extent.height;
    scene->camera.z_near = 0.1f;
    scene->camera.z_far = 100.0f;

    struct entity *cubes[] = {
        push_entity(scene),
        push_entity(scene),
    };
    cubes[0]->transform->position = { 10.0f, -3.0f, 15.0f };
    cubes[0]->mesh = ctk_at(&app->assets.meshes, "cube");
    cubes[0]->texture_desc_set = ctk_at(&app->descriptors.sets.textures, "wood");

    cubes[1]->transform->position = { 4.0f, 0.0f, 2.0f };
    cubes[1]->mesh = ctk_at(&app->assets.meshes, "cube");
    cubes[1]->texture_desc_set = ctk_at(&app->descriptors.sets.textures, "wood");

    struct entity *floor = push_entity(scene);
    floor->transform->rotation = { -90.0f, 0.0f, 0.0f };
    floor->transform->scale = { 32, 32, 1 };
    floor->mesh = ctk_at(&app->assets.meshes, "quad");
    floor->texture_desc_set = ctk_at(&app->descriptors.sets.textures, "wood");

    struct entity *sibenik = push_entity(scene);
    sibenik->transform->position = { 15.0f, -18.0f, 15.0f };
    sibenik->mesh = ctk_at(&app->assets.meshes, "sibenik");
    sibenik->texture_desc_set = ctk_at(&app->descriptors.sets.textures, "brick");

    struct light *light = push_light(scene);
    light->transform->position = { 8, -4, 15.5f };
    light->transform->rotation = { 0.0f, -90.0f, 0.0f };
    light->transform->scale = { 0.1f, 0.1f, 0.1f };
    light->ubo->depth_bias = 1;
    light->attenuation_index = 8;

    struct material *mat = push_material(scene, "test");
    mat->ubo->shine_exponent = 2;

    return scene;
}

static glm::mat4 camera_view_space_mtx(struct camera *cam) {
    struct transform *cam_trans = &cam->transform;

    // View Matrix
    glm::vec3 cam_pos = { cam_trans->position.x, cam_trans->position.y, cam_trans->position.z };
    glm::mat4 cam_mtx(1.0f);
    cam_mtx = glm::rotate(cam_mtx, glm::radians(cam_trans->rotation.x), { 1.0f, 0.0f, 0.0f });
    cam_mtx = glm::rotate(cam_mtx, glm::radians(cam_trans->rotation.y), { 0.0f, 1.0f, 0.0f });
    cam_mtx = glm::rotate(cam_mtx, glm::radians(cam_trans->rotation.z), { 0.0f, 0.0f, 1.0f });
    glm::vec3 cam_forward = { cam_mtx[0][2], cam_mtx[1][2], cam_mtx[2][2] };
    glm::mat4 view_mtx = glm::lookAt(cam_pos, cam_pos + cam_forward, { 0.0f, -1.0f, 0.0f });

    // Projection Matrix
    glm::mat4 proj_mtx = glm::perspective(glm::radians(cam->fov), cam->aspect, cam->z_near, cam->z_far);
    proj_mtx[1][1] *= -1; // Flip y value for scale (glm is designed for OpenGL).

    return proj_mtx * view_mtx;
}

static void update_lights(struct app *app, struct vk_core *vk, struct scene *scene, glm::mat4 *view_space_mtx, u32 swapchain_img_idx) {
    if (scene->lights.count == 0)
        return;

    for (u32 i = 0; i < scene->lights.count; ++i) {
        struct light *light = scene->lights + i;
        struct transform *trans = scene->light.transforms + i;
        struct model_ubo *model_ubo = scene->light.model_ubos + i;
        struct light_ubo *ubo = scene->light.ubos + i;

        struct ctk_v2<f32> const *atten_consts = LIGHT_ATTENUATION_CONSTS + light->attenuation_index;
        ubo->linear = atten_consts->x;
        ubo->quadratic = atten_consts->y;
        glm::vec3 light_pos = { trans->position.x, trans->position.y, trans->position.z };

        ////////////////////////////////////////////////////////////
        /// Light Model UBO
        ////////////////////////////////////////////////////////////

        // Model Matrix
        glm::mat4 model_mtx(1.0f);
        model_mtx = glm::translate(model_mtx, light_pos);
        model_mtx = glm::rotate(model_mtx, glm::radians(trans->rotation.x), { 1.0f, 0.0f, 0.0f });
        model_mtx = glm::rotate(model_mtx, glm::radians(trans->rotation.y), { 0.0f, 1.0f, 0.0f });
        model_mtx = glm::rotate(model_mtx, glm::radians(trans->rotation.z), { 0.0f, 0.0f, 1.0f });
        model_mtx = glm::scale(model_mtx, { trans->scale.x, trans->scale.y, trans->scale.z });
        model_ubo->model_mtx = model_mtx;
        model_ubo->mvp_mtx = *view_space_mtx * model_mtx;

        ////////////////////////////////////////////////////////////
        /// Light UBO
        ////////////////////////////////////////////////////////////
        ubo->pos = trans->position;
        // if (ubo->mode == LIGHT_MODE_DIRECTIONAL) {
        //     // View Matrix
        //     glm::mat4 view_mtx(1.0f);
        //     view_mtx = glm::rotate(view_mtx, glm::radians(trans->rotation.x), { 1.0f, 0.0f, 0.0f });
        //     view_mtx = glm::rotate(view_mtx, glm::radians(trans->rotation.y), { 0.0f, 1.0f, 0.0f });
        //     view_mtx = glm::rotate(view_mtx, glm::radians(trans->rotation.z), { 0.0f, 0.0f, 1.0f });
        //     glm::vec3 light_forward = { view_mtx[0][2], view_mtx[1][2], view_mtx[2][2] };
        //     view_mtx = glm::lookAt(light_pos, light_pos + light_forward, { 0.0f, -1.0f, 0.0f });

        //     // Projection Matrix
        //     f32 near_plane = 1.0f;
        //     f32 far_plane = 60.0f;
        //     glm::mat4 proj_mtx = glm::ortho(-30.0f, 30.0f, -30.0f, 30.0f, near_plane, far_plane);
        //     proj_mtx[1][1] *= -1;

        //     ubo->view_mtxs[0] = proj_mtx * view_mtx;
        //     ubo->direction = { light_forward.x, light_forward.y, light_forward.z };
        // } else {
        //     static glm::vec3 const DIRECTIONS[] = {
        //         { 1, 0, 0 },
        //         {-1, 0, 0 },
        //         { 0,-1, 0 },
        //         { 0, 1, 0 },
        //         { 0, 0, 1 },
        //         { 0, 0,-1 },
        //     };
        //     glm::mat4 proj_mtx = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 50.0f);
        //     proj_mtx[1][1] *= -1;
        //     for (u32 i = 0; i < 6; ++i) {
        //         glm::vec3 up = i == 2 ? glm::vec3(0, 0, -1) :
        //                        i == 3 ? glm::vec3(0, 0, 1) :
        //                        glm::vec3(0, -1, 0);
        //         ubo->view_mtxs[i] = proj_mtx * glm::lookAt(light_pos, light_pos + DIRECTIONS[i], up);
        //     }
        // }
    }
    vtk_write_to_host_region(vk->device.logical, scene->light.ubos.data, ctk_byte_count(&scene->light.ubos),
                             app->uniform_bufs.light_ubos.regions + swapchain_img_idx, 0);
    vtk_write_to_host_region(vk->device.logical, scene->light.model_ubos.data, ctk_byte_count(&scene->light.model_ubos),
                             app->uniform_bufs.light_model_ubos.regions + swapchain_img_idx, 0);
}

static void update_entities(struct app *app, struct vk_core *vk, struct scene *scene, glm::mat4 *view_space_mtx, u32 swapchain_img_idx) {
    if (scene->entities.count == 0)
        return;

    for (u32 i = 0; i < scene->entities.count; ++i) {
        struct transform *trans = scene->entity.transforms + i;
        struct model_ubo *model_ubo = scene->entity.model_ubos + i;
        glm::mat4 model_mtx(1.0f);
        model_mtx = glm::translate(model_mtx, { trans->position.x, trans->position.y, trans->position.z });
        model_mtx = glm::rotate(model_mtx, glm::radians(trans->rotation.x), { 1.0f, 0.0f, 0.0f });
        model_mtx = glm::rotate(model_mtx, glm::radians(trans->rotation.y), { 0.0f, 1.0f, 0.0f });
        model_mtx = glm::rotate(model_mtx, glm::radians(trans->rotation.z), { 0.0f, 0.0f, 1.0f });
        model_mtx = glm::scale(model_mtx, { trans->scale.x, trans->scale.y, trans->scale.z });

        model_ubo->model_mtx = model_mtx;
        model_ubo->mvp_mtx = *view_space_mtx * model_mtx;
    }
    vtk_write_to_host_region(vk->device.logical, scene->entity.model_ubos.data, ctk_byte_count(&scene->entity.model_ubos),
                             app->uniform_bufs.entity_model_ubos.regions + swapchain_img_idx, 0);
}

////////////////////////////////////////////////////////////
/// UI
////////////////////////////////////////////////////////////
enum {
    UI_MODE_ENTITY,
    UI_MODE_LIGHT,
    UI_MODE_MATERIAL,
};

struct ui {
    struct vtk_render_pass render_pass;
    VkDescriptorPool descriptor_pool;
    ImGuiIO *io;
    s32 mode;
    u32 entity_idx;
    u32 light_idx;
    u32 material_idx;
};

static void check_vk_result(VkResult result) {
    vtk_validate_result(result, "imgui internal call failed");
}

static struct ui *create_ui(struct window *win, struct app *app, struct vk_core *vk) {
    auto ui = ctk_zalloc<struct ui>();
    ui->mode = UI_MODE_LIGHT;

    ////////////////////////////////////////////////////////////
    /// Init
    ////////////////////////////////////////////////////////////
    ImGui::CreateContext();
    ui->io = &ImGui::GetIO();
    if (!ImGui_ImplGlfw_InitForVulkan(win->handle, true /* install callbacks so imgui can handle glfw input */))
        CTK_FATAL("failed to init imgui glfw for vulkan")

    ////////////////////////////////////////////////////////////
    /// Render Pass
    ////////////////////////////////////////////////////////////
    struct vtk_render_pass_info rp_info = {};

    // Attachments
    VkAttachmentDescription *swapchain_attachment = ctk_push(&rp_info.attachment_descriptions);
    swapchain_attachment->format = vk->swapchain.image_format;
    swapchain_attachment->samples = VK_SAMPLE_COUNT_1_BIT;
    swapchain_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    swapchain_attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    swapchain_attachment->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    swapchain_attachment->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    swapchain_attachment->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchain_attachment->finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    ctk_push(&rp_info.clear_values, { 0, 0, 0, 1 });

    // Subpasses
    struct vtk_subpass_info *main = ctk_push(&rp_info.subpass_infos);
    ctk_push(&main->color_attachment_refs, { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    // Subpass Dependencies
    rp_info.subpass_dependencies.count = 1;
    rp_info.subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    rp_info.subpass_dependencies[0].dstSubpass = 0;
    rp_info.subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    rp_info.subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    rp_info.subpass_dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    rp_info.subpass_dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    rp_info.subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Framebuffer Infos
    for (u32 i = 0; i < vk->swapchain.image_count; ++i) {
        struct vtk_framebuffer_info *fb_info = ctk_push(&rp_info.framebuffer_infos);
        ctk_push(&fb_info->attachments, vk->swapchain.image_views[i]);
        fb_info->extent = vk->swapchain.extent;
        fb_info->layers = 1;
    }

    ui->render_pass = vtk_create_render_pass(vk->device.logical, vk->graphics_cmd_pool, &rp_info);

    ////////////////////////////////////////////////////////////
    /// Descriptor Pool
    ////////////////////////////////////////////////////////////
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 16 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 16 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 },
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = 64;
    pool_info.poolSizeCount = CTK_ARRAY_COUNT(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    vtk_validate_result(vkCreateDescriptorPool(vk->device.logical, &pool_info, NULL, &ui->descriptor_pool), "failed to create descriptor pool");

    ////////////////////////////////////////////////////////////
    /// Vulkan Init
    ////////////////////////////////////////////////////////////
    ImGui_ImplVulkan_InitInfo imgui_vk_info = {};
    imgui_vk_info.Instance = vk->instance.handle;
    imgui_vk_info.PhysicalDevice = vk->device.physical;
    imgui_vk_info.Device = vk->device.logical;
    imgui_vk_info.QueueFamily = vk->device.queue_family_indexes.graphics;
    imgui_vk_info.Queue = vk->device.queues.graphics;
    imgui_vk_info.PipelineCache = VK_NULL_HANDLE;
    imgui_vk_info.DescriptorPool = ui->descriptor_pool;
    imgui_vk_info.MinImageCount = vk->swapchain.image_count;
    imgui_vk_info.ImageCount = vk->swapchain.image_count;
    imgui_vk_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    imgui_vk_info.Allocator = NULL;
    imgui_vk_info.CheckVkResultFn = check_vk_result;
    if (!ImGui_ImplVulkan_Init(&imgui_vk_info, ui->render_pass.handle))
        CTK_FATAL("failed to init imgui vulkan")

    ////////////////////////////////////////////////////////////
    /// Fonts
    ////////////////////////////////////////////////////////////
    vtk_begin_one_time_command_buffer(app->cmd_bufs.one_time);
        ImGui_ImplVulkan_CreateFontsTexture(app->cmd_bufs.one_time);
    vtk_submit_one_time_command_buffer(app->cmd_bufs.one_time, vk->device.queues.graphics);
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    return ui;
}

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

static void window_end() {
    ImGui::End();
}

static void separator() {
    ImVec2 output_pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(output_pos.x, output_pos.y + 1),
                                        ImVec2(output_pos.x + ImGui::GetColumnWidth(), output_pos.y + 1),
                                        IM_COL32(255, 255, 255, 64));
    ImGui::Dummy(ImVec2(0, 2));
}

static void transform_control(struct transform *t) {
    ImGui::Text("transform");
    ImGui::DragFloat3("position", &t->position.x, 0.01f);
    ImGui::DragFloat3("rotation", &t->rotation.x, 0.1f);
    ImGui::DragFloat3("scale", &t->scale.x, 0.01f);
}

static void enum_dropdown(cstr id, cstr *elems, u32 elem_count, s32 *enum_val) {
    cstr curr_elem = elems[*enum_val];
    ImGui::PushItemWidth(-1);
    char imgui_id[256] = {};
    sprintf(imgui_id, "##%s", id);
    if (ImGui::BeginCombo(imgui_id, curr_elem)) {
        for (u32 i = 0; i < elem_count; ++i) {
            cstr elem = elems[i];
            bool selected = curr_elem == elem;
            if (ImGui::Selectable(elem, selected))
                *enum_val = i;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
}

static void draw_ui(struct ui *ui, struct scene *scene, struct window *win) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    s32 window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove/* | ImGuiWindowFlags_NoResize*/;
    if (window_begin("states", 0, 0, 600, (s32)win->height, window_flags)) {
        ImGui::Columns(2, NULL);

        static cstr UI_MODES[] = { "entities", "lights", "materials" };
        enum_dropdown("ui_modes", UI_MODES, CTK_ARRAY_COUNT(UI_MODES), &ui->mode);

        if (ui->mode == UI_MODE_ENTITY) {
            if (list_box_begin("0", NULL, scene->entities.count)) {
                for (u32 i = 0; i < scene->entities.count; ++i) {
                    cstr entity_name = scene->entities[i].name;
                    char name[64] = {};
                    if (entity_name != NULL)
                        strcpy(name, entity_name);
                    else
                        sprintf(name, "entity %u", i);
                    if (ImGui::Selectable(name, i == ui->entity_idx))
                        ui->entity_idx = i;
                }
            }
            list_box_end();
        } else if (ui->mode == UI_MODE_LIGHT) {
            if (list_box_begin("1", NULL, scene->lights.count)) {
                for (u32 i = 0; i < scene->lights.count; ++i) {
                    char name[16] = {};
                    sprintf(name, "light %u", i);
                    if (ImGui::Selectable(name, i == ui->light_idx))
                        ui->light_idx = i;
                }
            }
            list_box_end();
        } else if (ui->mode == UI_MODE_MATERIAL) {
            if (list_box_begin("2", NULL, scene->materials.count)) {
                for (u32 i = 0; i < scene->materials.count; ++i) {
                    cstr mat_name = scene->materials[i].name;
                    char name[64] = {};
                    if (mat_name != NULL)
                        strcpy(name, mat_name);
                    else
                        sprintf(name, "material %u", i);
                    if (ImGui::Selectable(name, i == ui->material_idx))
                        ui->material_idx = i;
                }
            }
            list_box_end();
        }

        ImGui::NextColumn();
        if (ui->mode == UI_MODE_ENTITY) {
            struct entity *entity = scene->entities + ui->entity_idx;
            transform_control(entity->transform);
        } else if (ui->mode == UI_MODE_LIGHT) {
            struct light *light = scene->lights + ui->light_idx;
            struct light_ubo *ubo = scene->light.ubos + ui->light_idx;
            transform_control(scene->light.transforms + ui->light_idx);

            static cstr LIGHT_MODE_NAMES[] = { "directional", "point" };
            enum_dropdown("light_modes", LIGHT_MODE_NAMES, CTK_ARRAY_COUNT(LIGHT_MODE_NAMES), &ubo->mode);
            ImGui::InputFloat("far_clip", &ubo->far_clip);
            ImGui::InputInt("depth_bias", &ubo->depth_bias);
            ImGui::InputInt("normal_bias", &ubo->normal_bias);
            ImGui::SliderInt("attenuation_index", (s32*)&light->attenuation_index, 0, CTK_ARRAY_COUNT(LIGHT_ATTENUATION_CONSTS) - 1);
            ImGui::SliderFloat("ambient", &light->ubo->ambient, 0, 1, "%.2f");
            ImGui::ColorPicker4("##color", &ubo->color.x);
        } else if (ui->mode == UI_MODE_MATERIAL) {
            struct material *mat = scene->materials + ui->material_idx;
            ImGui::SliderInt("shine_exponent", (s32 *)&mat->ubo->shine_exponent, 1, 256);
        }
    }
    window_end();
}

////////////////////////////////////////////////////////////
/// Controls
////////////////////////////////////////////////////////////
static void update_mouse_state(struct window *window) {
    // Mouse Delta
    struct ctk_v2<f64> prev_mouse_position = window->mouse_position;
    f64 curr_mouse_x = 0.0;
    f64 curr_mouse_y = 0.0;
    glfwGetCursorPos(window->handle, &curr_mouse_x, &curr_mouse_y);
    window->mouse_position = { curr_mouse_x, curr_mouse_y };

    // Calculate delta if previous position was not unset.
    if (prev_mouse_position != UNSET_MOUSE_POSITION)
        window->mouse_delta = window->mouse_position - prev_mouse_position;
}

static void local_translate(struct transform *transform, struct ctk_v3<f32> translation) {
    struct ctk_v3<f32> *pos = &transform->position;
    struct ctk_v3<f32> *rot = &transform->rotation;

    glm::mat4 world_mtx(1.0f);
    world_mtx = glm::rotate(world_mtx, glm::radians(rot->x), { 1.0f, 0.0f, 0.0f });
    world_mtx = glm::rotate(world_mtx, glm::radians(rot->y), { 0.0f, 1.0f, 0.0f });
    world_mtx = glm::rotate(world_mtx, glm::radians(rot->z), { 0.0f, 0.0f, 1.0f });
    world_mtx = glm::translate(world_mtx, { pos->x, pos->y, pos->z });

    struct ctk_v3<f32> right = {};
    right.x = world_mtx[0][0];
    right.y = world_mtx[1][0];
    right.z = world_mtx[2][0];

    struct ctk_v3<f32> up = {};
    up.x = world_mtx[0][1];
    up.y = world_mtx[1][1];
    up.z = world_mtx[2][1];

    struct ctk_v3<f32> forward = {};
    forward.x = world_mtx[0][2];
    forward.y = world_mtx[1][2];
    forward.z = world_mtx[2][2];

    struct ctk_v3<f32> new_pos = *pos;
    new_pos = new_pos + (right * translation.x);
    new_pos = new_pos + (up * translation.y);
    *pos = new_pos + (forward * translation.z);
}

static void camera_controls(struct transform *cam_trans, struct window *window) {
    if (window->mouse_button_down[GLFW_MOUSE_BUTTON_2]) {
        static f32 const SENS = 0.4f;
        cam_trans->rotation.x += window->mouse_delta.y * SENS;
        cam_trans->rotation.y -= window->mouse_delta.x * SENS;
        cam_trans->rotation.x = ctk_clamp(cam_trans->rotation.x, -80.0f, 80.0f);
    }

    struct ctk_v3<f32> translation = {};
    f32 mod = window->key_down[GLFW_KEY_LEFT_SHIFT] ? 4 :
              window->key_down[GLFW_KEY_LEFT_CONTROL] ? 1 :
              2;
    if (window->key_down[GLFW_KEY_W]) translation.z += 0.01f * mod;
    if (window->key_down[GLFW_KEY_S]) translation.z -= 0.01f * mod;
    if (window->key_down[GLFW_KEY_D]) translation.x += 0.01f * mod;
    if (window->key_down[GLFW_KEY_A]) translation.x -= 0.01f * mod;
    if (window->key_down[GLFW_KEY_E]) translation.y -= 0.01f * mod;
    if (window->key_down[GLFW_KEY_Q]) translation.y += 0.01f * mod;
    local_translate(cam_trans, translation);
}

////////////////////////////////////////////////////////////
/// Rendering
////////////////////////////////////////////////////////////
static u32 vtk_aquire_swapchain_image_index(struct app *app, struct vk_core *vk) {
    u32 swapchain_img_idx = CTK_U32_MAX;
    VkSemaphore img_aquired_semaphore = app->frame_sync.img_aquired[app->frame_sync.curr_frame];
    vtk_validate_result(vkAcquireNextImageKHR(vk->device.logical, vk->swapchain.handle, CTK_U64_MAX, img_aquired_semaphore, VK_NULL_HANDLE, &swapchain_img_idx),
                        "failed to aquire next swapchain image");
    return swapchain_img_idx;
}

static void sync_frame(struct app *app, struct vk_core *vk, u32 swapchain_img_idx) {
    u32 *img_prev_frame = app->frame_sync.img_prev_frame + swapchain_img_idx;
    if (*img_prev_frame != CTK_U32_MAX)
        vkWaitForFences(vk->device.logical, 1, app->frame_sync.in_flight + *img_prev_frame, VK_TRUE, CTK_U64_MAX);
    vkResetFences(vk->device.logical, 1, app->frame_sync.in_flight + app->frame_sync.curr_frame);
    *img_prev_frame = app->frame_sync.curr_frame;
}

static void render_omni_shadow_map_direction(struct app *app, struct vk_core *vk, struct scene *scene, u32 swapchain_img_idx, VkCommandBuffer cmd_buf,
                                             u32 direction_view_mtx_idx) {
    struct vtk_render_pass *rp = &app->render_passes.shadow;

    VkRect2D render_area = {};
    render_area.offset.x = 0;
    render_area.offset.y = 0;
    render_area.extent.width = SHADOW_MAP_SIZE;
    render_area.extent.height = SHADOW_MAP_SIZE;

    VkRenderPassBeginInfo rp_begin_info = {};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = rp->handle;
    rp_begin_info.framebuffer = rp->framebuffers[swapchain_img_idx];
    rp_begin_info.renderArea = render_area;
    rp_begin_info.clearValueCount = rp->clear_values.count;
    rp_begin_info.pClearValues = rp->clear_values.data;

    vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        struct vtk_graphics_pipeline *gp = &app->graphics_pipelines.perspective_shadow;
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, gp->handle);

        // Push Constants
        vkCmdPushConstants(cmd_buf, gp->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(u32), &direction_view_mtx_idx);

        // Light Descriptor Sets
        struct vtk_descriptor_set_binding light_desc_set_binding = { &app->descriptors.sets.light_ubo, { 0u }, swapchain_img_idx };
        vtk_bind_descriptor_sets(cmd_buf, gp->layout, 0, &light_desc_set_binding, 1);

        for (u32 i = 0; i < scene->entities.count; ++i) {
            struct entity *entity = scene->entities + i;
            struct mesh *mesh = entity->mesh;

            // Entity Descriptor Sets
            struct vtk_descriptor_set_binding entity_desc_set_binding = { &app->descriptors.sets.entity_model_ubo, { i }, swapchain_img_idx };
            vtk_bind_descriptor_sets(cmd_buf, gp->layout, 1, &entity_desc_set_binding, 1);

            vkCmdBindVertexBuffers(cmd_buf, 0, 1, &mesh->vertex_region.buffer->handle, &mesh->vertex_region.offset);
            vkCmdBindIndexBuffer(cmd_buf, mesh->index_region.buffer->handle, mesh->index_region.offset, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd_buf, mesh->indexes.count, 1, 0, 0, 0);
        }
    vkCmdEndRenderPass(cmd_buf);
}

static void copy_omni_shadow_map_face(struct app *app, VkCommandBuffer cmd_buf, u32 face_idx) {
    // Transition directional shadow map to transfer source.
    {
        VkImageMemoryBarrier img_barrier = {};
        img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        img_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barrier.image = app->shadow_maps.directional.handle;
        img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        img_barrier.subresourceRange.baseMipLevel = 0;
        img_barrier.subresourceRange.levelCount = 1;
        img_barrier.subresourceRange.baseArrayLayer = 0;
        img_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd_buf,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             0, // Dependency Flags
                             0, NULL, // Memory Barriers
                             0, NULL, // Buffer Memory Barriers
                             1, &img_barrier); // Image Memory Barriers
    }

    // Copy from directional shadow map to face of omni shadow map.
    VkImageCopy copy_region = {};
    copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    copy_region.srcSubresource.baseArrayLayer = 0;
    copy_region.srcSubresource.mipLevel = 0;
    copy_region.srcSubresource.layerCount = 1;
    copy_region.srcOffset = { 0, 0, 0 };
    copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    copy_region.dstSubresource.baseArrayLayer = face_idx;
    copy_region.dstSubresource.mipLevel = 0;
    copy_region.dstSubresource.layerCount = 1;
    copy_region.dstOffset = { 0, 0, 0 };
    copy_region.extent.width = SHADOW_MAP_SIZE;
    copy_region.extent.height = SHADOW_MAP_SIZE;
    copy_region.extent.depth = 1;

    // Copy from directional shadow map to omni shadow map face.
    vkCmdCopyImage(cmd_buf,
                   app->shadow_maps.directional.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   app->shadow_maps.omni.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &copy_region);

    // Transition directional shadow map back to depth attachment.
    {
        VkImageMemoryBarrier img_barrier = {};
        img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        img_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        img_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barrier.image = app->shadow_maps.directional.handle;
        img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        img_barrier.subresourceRange.baseMipLevel = 0;
        img_barrier.subresourceRange.levelCount = 1;
        img_barrier.subresourceRange.baseArrayLayer = 0;
        img_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd_buf,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             0, // Dependency Flags
                             0, NULL, // Memory Barriers
                             0, NULL, // Buffer Memory Barriers
                             1, &img_barrier); // Image Memory Barriers
    }
}

static void record_render_passes(struct app *app, struct vk_core *vk, struct scene *scene, struct ui *ui, u32 swapchain_img_idx) {
    VkCommandBufferBeginInfo cmd_buf_begin_info = {};
    cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_begin_info.flags = 0;
    cmd_buf_begin_info.pInheritanceInfo = NULL;

    VkCommandBuffer cmd_buf = app->cmd_bufs.render[swapchain_img_idx];
    vtk_validate_result(vkBeginCommandBuffer(cmd_buf, &cmd_buf_begin_info), "failed to begin recording command buffer");
#if 1
        // Shadow
        {
            // Calculate Light Matrix
            struct light *light = scene->lights + 0;
            float near_clip = 0.1f;
            float far_clip = 50.0f;
            glm::vec3 light_pos = { light->transform->position.x, light->transform->position.y, light->transform->position.z };
            glm::mat4 view_mtx = glm::lookAt(light_pos, light_pos + glm::vec3({ 1, 0, 0 }), { 0, -1, 0 });
            glm::mat4 proj_mtx = glm::perspective(glm::radians(90.0f), 1.0f, near_clip, far_clip);
            proj_mtx[1][1] *= -1;
            glm::mat4 light_mtx = proj_mtx * view_mtx;

            // Render Shadow Map
            struct vtk_render_pass *rp = &app->render_passes.shadow;

            VkRect2D render_area = {};
            render_area.offset.x = 0;
            render_area.offset.y = 0;
            render_area.extent.width = SHADOW_MAP_SIZE;
            render_area.extent.height = SHADOW_MAP_SIZE;

            VkRenderPassBeginInfo rp_begin_info = {};
            rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin_info.renderPass = rp->handle;
            rp_begin_info.framebuffer = rp->framebuffers[swapchain_img_idx];
            rp_begin_info.renderArea = render_area;
            rp_begin_info.clearValueCount = rp->clear_values.count;
            rp_begin_info.pClearValues = rp->clear_values.data;

            vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
                struct vtk_graphics_pipeline *gp = &app->graphics_pipelines.perspective_shadow;
                vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, gp->handle);

                // Push Constants
                vkCmdPushConstants(cmd_buf, gp->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 64, &light_mtx);
                vkCmdPushConstants(cmd_buf, gp->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 64, 12, &light_pos);
                vkCmdPushConstants(cmd_buf, gp->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 76, 4, &far_clip);

                for (u32 i = 0; i < scene->entities.count; ++i) {
                    struct entity *entity = scene->entities + i;
                    struct mesh *mesh = entity->mesh;

                    // Entity Descriptor Sets
                    struct vtk_descriptor_set_binding entity_desc_set_binding = { &app->descriptors.sets.entity_model_ubo, { i }, swapchain_img_idx };
                    vtk_bind_descriptor_sets(cmd_buf, gp->layout, 0, &entity_desc_set_binding, 1);

                    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &mesh->vertex_region.buffer->handle, &mesh->vertex_region.offset);
                    vkCmdBindIndexBuffer(cmd_buf, mesh->index_region.buffer->handle, mesh->index_region.offset, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(cmd_buf, mesh->indexes.count, 1, 0, 0, 0);
                }
            vkCmdEndRenderPass(cmd_buf);

            // // Transition omni shadow map to transfer destination.
            // {
            //     VkImageMemoryBarrier img_barrier = {};
            //     img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            //     img_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            //     img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            //     img_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            //     img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            //     img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            //     img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            //     img_barrier.image = app->shadow_maps.omni.handle;
            //     img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            //     img_barrier.subresourceRange.baseMipLevel = 0;
            //     img_barrier.subresourceRange.levelCount = 1;
            //     img_barrier.subresourceRange.baseArrayLayer = 0;
            //     img_barrier.subresourceRange.layerCount = 6;
            //     vkCmdPipelineBarrier(cmd_buf,
            //                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            //                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            //                          0, // Dependency Flags
            //                          0, NULL, // Memory Barriers
            //                          0, NULL, // Buffer Memory Barriers
            //                          1, &img_barrier); // Image Memory Barriers
            // }

            // // Render shadow maps for each direcion and copy to respective omni shadow map face.
            // for (u32 i = 0; i < 6; ++i) {
            //     render_omni_shadow_map_direction(app, vk, scene, swapchain_img_idx, cmd_buf, i);
            //     copy_omni_shadow_map_face(app, cmd_buf, i);
            // }

            // // Transition omni shadow map back to shader readonly.
            // {
            //     VkImageMemoryBarrier img_barrier = {};
            //     img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            //     img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            //     img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            //     img_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            //     img_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            //     img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            //     img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            //     img_barrier.image = app->shadow_maps.omni.handle;
            //     img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            //     img_barrier.subresourceRange.baseMipLevel = 0;
            //     img_barrier.subresourceRange.levelCount = 1;
            //     img_barrier.subresourceRange.baseArrayLayer = 0;
            //     img_barrier.subresourceRange.layerCount = 6;
            //     vkCmdPipelineBarrier(cmd_buf,
            //                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            //                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            //                          0, // Dependency Flags
            //                          0, NULL, // Memory Barriers
            //                          0, NULL, // Buffer Memory Barriers
            //                          1, &img_barrier); // Image Memory Barriers
            // }
        }
#endif
#if 1
        // Direct
        {
            struct vtk_render_pass *rp = &app->render_passes.direct;

            VkRect2D render_area = {};
            render_area.offset.x = 0;
            render_area.offset.y = 0;
            render_area.extent = vk->swapchain.extent;

            VkRenderPassBeginInfo rp_begin_info = {};
            rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin_info.renderPass = rp->handle;
            rp_begin_info.framebuffer = rp->framebuffers[swapchain_img_idx];
            rp_begin_info.renderArea = render_area;
            rp_begin_info.clearValueCount = rp->clear_values.count;
            rp_begin_info.pClearValues = rp->clear_values.data;

            vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
                ////////////////////////////////////////////////////////////
                /// Render Entities
                ////////////////////////////////////////////////////////////
                struct vtk_graphics_pipeline *direct_gp = &app->graphics_pipelines.direct;
                vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, direct_gp->handle);

                // Push Constants
                vkCmdPushConstants(cmd_buf, direct_gp->layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(struct ctk_v3<f32>), &scene->camera.transform.position);

                // Light Descriptor Sets
                struct vtk_descriptor_set_binding light_desc_set_binding = { &app->descriptors.sets.light_ubo, { 0u }, swapchain_img_idx };
                vtk_bind_descriptor_sets(cmd_buf, direct_gp->layout, 0, &light_desc_set_binding, 1);

                for (u32 i = 0; i < scene->entities.count; ++i) {
                    struct entity *entity = scene->entities + i;
                    struct mesh *mesh = entity->mesh;

                    // Entity Descriptor Sets
                    struct vtk_descriptor_set_binding entity_desc_set_bindings[] = {
                        { &app->descriptors.sets.entity_model_ubo, { i }, swapchain_img_idx },
                        { entity->texture_desc_set },
                        { &app->descriptors.sets.shadow_maps.directional },
                        { &app->descriptors.sets.shadow_maps.omni },
                    };
                    vtk_bind_descriptor_sets(cmd_buf, direct_gp->layout, 1, entity_desc_set_bindings, CTK_ARRAY_COUNT(entity_desc_set_bindings));

                    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &mesh->vertex_region.buffer->handle, &mesh->vertex_region.offset);
                    vkCmdBindIndexBuffer(cmd_buf, mesh->index_region.buffer->handle, mesh->index_region.offset, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(cmd_buf, mesh->indexes.count, 1, 0, 0, 0);
                }

                ////////////////////////////////////////////////////////////
                /// Render Lights
                ////////////////////////////////////////////////////////////
                struct vtk_graphics_pipeline *unlit_gp = &app->graphics_pipelines.unlit;
                vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, unlit_gp->handle);

                struct mesh *light_diamond = ctk_at(&app->assets.meshes, "light_diamond");
                for (u32 i = 0; i < scene->lights.count; ++i) {
                    struct vtk_descriptor_set_binding desc_set_bindings[] = {
                        { &app->descriptors.sets.light_ubo, { i }, swapchain_img_idx },
                        { &app->descriptors.sets.light_model_ubo, { i }, swapchain_img_idx },
                    };
                    vtk_bind_descriptor_sets(cmd_buf, unlit_gp->layout, 0, desc_set_bindings, CTK_ARRAY_COUNT(desc_set_bindings));
                    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &light_diamond->vertex_region.buffer->handle, &light_diamond->vertex_region.offset);
                    vkCmdBindIndexBuffer(cmd_buf, light_diamond->index_region.buffer->handle, light_diamond->index_region.offset, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(cmd_buf, light_diamond->indexes.count, 1, 0, 0, 0);
                }
            vkCmdEndRenderPass(cmd_buf);
        }
#endif
#if 1
        // Fullscreen Texture
        {
            struct vtk_render_pass *rp = &app->render_passes.fullscreen_texture;

            VkRect2D render_area = {};
            render_area.offset.x = 0;
            render_area.offset.y = 0;
            render_area.extent = vk->swapchain.extent;

            VkRenderPassBeginInfo rp_begin_info = {};
            rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin_info.renderPass = rp->handle;
            rp_begin_info.framebuffer = rp->framebuffers[swapchain_img_idx];
            rp_begin_info.renderArea = render_area;
            rp_begin_info.clearValueCount = rp->clear_values.count;
            rp_begin_info.pClearValues = rp->clear_values.data;

            vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
                struct vtk_graphics_pipeline *gp = &app->graphics_pipelines.fullscreen_texture;
                struct mesh *fullscreen_quad = ctk_at(&app->assets.meshes, "fullscreen_quad");
                vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, gp->handle);
                struct vtk_descriptor_set_binding desc_set_bindings[] = {
                    { &app->descriptors.sets.shadow_maps.directional },
                };
                vtk_bind_descriptor_sets(cmd_buf, gp->layout, 0, desc_set_bindings, CTK_ARRAY_COUNT(desc_set_bindings));
                vkCmdBindVertexBuffers(cmd_buf, 0, 1, &fullscreen_quad->vertex_region.buffer->handle, &fullscreen_quad->vertex_region.offset);
                vkCmdBindIndexBuffer(cmd_buf, fullscreen_quad->index_region.buffer->handle, fullscreen_quad->index_region.offset, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd_buf, fullscreen_quad->indexes.count, 1, 0, 0, 0);
            vkCmdEndRenderPass(cmd_buf);
        }
#endif
#if 1
        // UI
        {
            struct vtk_render_pass *rp = &ui->render_pass;
            ImGui::Render();

            VkRect2D render_area = {};
            render_area.offset.x = 0;
            render_area.offset.y = 0;
            render_area.extent = vk->swapchain.extent;

            VkRenderPassBeginInfo rp_begin_info = {};
            rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin_info.renderPass = rp->handle;
            rp_begin_info.framebuffer = rp->framebuffers[swapchain_img_idx];
            rp_begin_info.renderArea = render_area;
            rp_begin_info.clearValueCount = rp->clear_values.count;
            rp_begin_info.pClearValues = rp->clear_values.data;

            vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf);
            vkCmdEndRenderPass(cmd_buf);
        }
#endif
    vtk_validate_result(vkEndCommandBuffer(cmd_buf), "error during render pass command recording");
}

static void submit_command_buffers(struct app *app, struct vk_core *vk, u32 swapchain_img_idx) {
    // Render
    {
        VkCommandBuffer cmd_bufs[] = {
            app->cmd_bufs.render[swapchain_img_idx],
        };
        VkSemaphore wait_semaphores[] = {
            app->frame_sync.img_aquired[app->frame_sync.curr_frame],
        };
        VkPipelineStageFlags wait_stages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkSemaphore signal_semaphores[] = {
            app->frame_sync.render_finished[app->frame_sync.curr_frame],
        };

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = CTK_ARRAY_COUNT(wait_semaphores);
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = CTK_ARRAY_COUNT(cmd_bufs);
        submit_info.pCommandBuffers = cmd_bufs;
        submit_info.signalSemaphoreCount = CTK_ARRAY_COUNT(signal_semaphores);
        submit_info.pSignalSemaphores = signal_semaphores;
        vtk_validate_result(vkQueueSubmit(vk->device.queues.graphics, 1, &submit_info, app->frame_sync.in_flight[app->frame_sync.curr_frame]),
                            "failed to submit %u command buffer(s) to graphics queue", submit_info.commandBufferCount);
    }

    // Present
    {
        VkSemaphore wait_semaphores[] = {
            app->frame_sync.render_finished[app->frame_sync.curr_frame],
        };
        VkSwapchainKHR swapchains[] = {
            vk->swapchain.handle,
        };
        u32 swapchain_img_idxs[] = {
            swapchain_img_idx,
        };

        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = CTK_ARRAY_COUNT(wait_semaphores);
        present_info.pWaitSemaphores = wait_semaphores;
        present_info.swapchainCount = CTK_ARRAY_COUNT(swapchains);
        present_info.pSwapchains = swapchains;
        present_info.pImageIndices = swapchain_img_idxs;
        present_info.pResults = NULL;
        vtk_validate_result(vkQueuePresentKHR(vk->device.queues.present, &present_info), "failed to queue image for presentation");
    }
}

static void cycle_frame(struct app *app) {
    app->frame_sync.curr_frame = app->frame_sync.curr_frame == app->frame_sync.frame_count - 1 ? 0 : app->frame_sync.curr_frame + 1;
}

////////////////////////////////////////////////////////////
/// Main
////////////////////////////////////////////////////////////
void test_main() {
    struct window *win = create_window();
    struct vk_core *vk = create_vk_core(win);
    struct app *app = create_app(vk);
    struct scene *scene = create_scene(app, vk);
    struct ui *ui = create_ui(win, app, vk);
    while (!glfwWindowShouldClose(win->handle)) {
        // Input
        glfwPollEvents();
        if (win->key_down[GLFW_KEY_ESCAPE])
            break;
        update_mouse_state(win);
        camera_controls(&scene->camera.transform, win);

        // Rendering
        u32 swapchain_img_idx = vtk_aquire_swapchain_image_index(app, vk);
        sync_frame(app, vk, swapchain_img_idx);
        draw_ui(ui, scene, win);
        glm::mat4 view_space_mtx = camera_view_space_mtx(&scene->camera);
        update_lights(app, vk, scene, &view_space_mtx, swapchain_img_idx);
        update_entities(app, vk, scene, &view_space_mtx, swapchain_img_idx);
        record_render_passes(app, vk, scene, ui, swapchain_img_idx);
        submit_command_buffers(app, vk, swapchain_img_idx);
        cycle_frame(app);

        Sleep(1);
    }
}
