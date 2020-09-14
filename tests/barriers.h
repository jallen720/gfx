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

#include "ctk/ctk_new.h"
#include "vtk/vtk_new.h"

////////////////////////////////////////////////////////////
/// Window
////////////////////////////////////////////////////////////
static struct ctk_v2d const UNSET_MOUSE_POSITION = { -10000.0, -10000.0 };

struct window {
    GLFWwindow *handle;
    u32 width;
    u32 height;
    bool key_down[GLFW_KEY_LAST + 1];
    bool mouse_button_down[GLFW_MOUSE_BUTTON_LAST + 1];
    struct ctk_v2d mouse_position = UNSET_MOUSE_POSITION;
    struct ctk_v2d mouse_delta;
};

static void error_callback(s32 err, cstr msg) {
    CTK_FATAL("[%d] %s", err, msg)
}

static void key_callback(GLFWwindow* glfw_win, s32 key, s32 scancode, s32 action, s32 mods) {
    auto window = (struct window *)glfwGetWindowUserPointer(glfw_win);
    window->key_down[key] = action == GLFW_PRESS || action == GLFW_REPEAT;
}

static void mouse_button_callback(GLFWwindow* glfw_win, s32 button, s32 action, s32 mods) {
    auto window = (struct window *)glfwGetWindowUserPointer(glfw_win);
    window->mouse_button_down[button] = action == GLFW_PRESS || action == GLFW_REPEAT;
}

static void init_window(struct window *window) {
    glfwSetErrorCallback(error_callback);
    if (glfwInit() != GLFW_TRUE)
        CTK_FATAL("failed to init glfw")
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window->width = 1600;
    window->height = 900;
    window->handle = glfwCreateWindow(window->width, window->height, "test", NULL, NULL);
    if (window->handle == NULL)
        CTK_FATAL("failed to create window")
    glfwSetWindowPos(window->handle, 320, 60);
    glfwSetWindowUserPointer(window->handle, window);
    glfwSetKeyCallback(window->handle, key_callback);
    glfwSetMouseButtonCallback(window->handle, mouse_button_callback);
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
    VkCommandBuffer one_time_cmd_buf;
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

static void init_vk_core(struct vk_core *vk, struct window *window) {
    vk->instance = vtk_create_instance();
    vtk_validate_result(glfwCreateWindowSurface(vk->instance.handle, window->handle, NULL, &vk->surface),
                        "failed to create glfw surface");

    VkPhysicalDeviceFeatures features = {};
    features.geometryShader = VK_TRUE;
    features.samplerAnisotropy = VK_TRUE;
    vk->device = vtk_create_device(vk->instance.handle, vk->surface, &features);

    vk->swapchain = vtk_create_swapchain(&vk->device, vk->surface);

    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_info.queueFamilyIndex = vk->device.queue_family_indexes.graphics;
    vtk_validate_result(vkCreateCommandPool(vk->device.logical, &cmd_pool_info, NULL, &vk->graphics_cmd_pool),
                        "failed to create command pool");

    create_buffers(vk);
    vk->staging_region = vtk_allocate_region(&vk->buffers.host, 64 * CTK_MEGABYTE);
    vk->one_time_cmd_buf = vtk_allocate_command_buffer(vk->device.logical, vk->graphics_cmd_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

////////////////////////////////////////////////////////////
/// Assets
////////////////////////////////////////////////////////////
struct asset_info {
    cstr name;
    cstr path;
};

struct shader_info : public asset_info {
    VkShaderStageFlagBits stage;
};

struct texture {
    VkImage image;
    VkDeviceMemory memory;
    VkSampler sampler;
};

struct vertex {
    struct ctk_v3 position;
    struct ctk_v3 normal;
    struct ctk_v2 uv;
};

struct mesh {
    struct ctk_buffer<struct vertex> vertexes;
    struct ctk_buffer<u32> indexes;
    struct vtk_region vertex_region;
    struct vtk_region index_region;
};

struct assets {
    struct ctk_map<struct vtk_shader, 16> shaders;
    struct ctk_map<struct texture, 16> textures;
    struct ctk_map<struct mesh, 16> meshes;
};

static void load_mesh(struct mesh *mesh, cstr path, struct vk_core *vk) {
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
    vtk_begin_one_time_command_buffer(vk->one_time_cmd_buf);
        vtk_write_to_device_region(&vk->device, vk->one_time_cmd_buf, mesh->vertexes.data, verts_byte_size,
                                   &vk->staging_region, 0, &mesh->vertex_region, 0);
        vtk_write_to_device_region(&vk->device, vk->one_time_cmd_buf, mesh->indexes.data, idxs_byte_size,
                                   &vk->staging_region, verts_byte_size, &mesh->index_region, 0);
    vtk_submit_one_time_command_buffer(vk->one_time_cmd_buf, vk->device.queues.graphics);

    // Cleanup
    aiReleaseImport(scene);
}

static void load_texture(struct texture *tex, cstr path, struct vk_core *vk) {
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

    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = 0;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = NULL; // Ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT.
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vtk_validate_result(vkCreateImage(vk->device.logical, &image_info, NULL, &tex->image), "failed to create image");

    VkMemoryRequirements mem_reqs = {};
    vkGetImageMemoryRequirements(vk->device.logical, tex->image, &mem_reqs);
    tex->memory = vtk_allocate_device_memory(&vk->device, &mem_reqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vtk_validate_result(vkBindImageMemory(vk->device.logical, tex->image, tex->memory, 0), "failed to bind image memory");

    // Copy image data (now in staging region) to texture image memory, transitioning texture image layout with pipeline barriers as
    // necessary.
    vtk_begin_one_time_command_buffer(vk->one_time_cmd_buf);
        VkImageMemoryBarrier pre_mem_barrier = {};
        pre_mem_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        pre_mem_barrier.srcAccessMask = 0;
        pre_mem_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        pre_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        pre_mem_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        pre_mem_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre_mem_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre_mem_barrier.image = tex->image;
        pre_mem_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        pre_mem_barrier.subresourceRange.baseMipLevel = 0;
        pre_mem_barrier.subresourceRange.levelCount = 1;
        pre_mem_barrier.subresourceRange.baseArrayLayer = 0;
        pre_mem_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(vk->one_time_cmd_buf,
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
        vkCmdCopyBufferToImage(vk->one_time_cmd_buf, vk->staging_region.buffer->handle, tex->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        VkImageMemoryBarrier post_mem_barrier = {};
        post_mem_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        post_mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        post_mem_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        post_mem_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        post_mem_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        post_mem_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post_mem_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        post_mem_barrier.image = tex->image;
        post_mem_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        post_mem_barrier.subresourceRange.baseMipLevel = 0;
        post_mem_barrier.subresourceRange.levelCount = 1;
        post_mem_barrier.subresourceRange.baseArrayLayer = 0;
        post_mem_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(vk->one_time_cmd_buf,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, // Dependency Flags
                             0, NULL, // Memory Barriers
                             0, NULL, // Buffer Memory Barriers
                             1, &post_mem_barrier); // Image Memory Barriers
    vtk_submit_one_time_command_buffer(vk->one_time_cmd_buf, vk->device.queues.graphics);
}

static void load_assets(struct assets *assets, struct vk_core *vk) {
    // Shaders
    struct shader_info shader_infos[] = {
        { "barriers_shadow_vert", "assets/shaders/barriers/shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT },
        { "barriers_shadow_frag", "assets/shaders/barriers/shadow.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT },
    };
    for (u32 i = 0; i < CTK_ARRAY_COUNT(shader_infos); ++i) {
        ctk_push(&assets->shaders, shader_infos[i].name,
                 vtk_create_shader(vk->device.logical, shader_infos[i].path, shader_infos[i].stage));
    }

    // Textures
    struct asset_info texture_infos[] = {
        { "wood", "assets/textures/wood.png" },
    };
    for (u32 i = 0; i < CTK_ARRAY_COUNT(texture_infos); ++i)
        load_texture(ctk_push(&assets->textures, texture_infos[i].name), texture_infos[i].path, vk);

    // Meshes
    struct asset_info mesh_infos[] = {
        { "cube", "assets/models/cube.obj" },
        { "true_cube", "assets/models/true_cube.obj" },
        { "quad", "assets/models/quad.obj" },
    };
    for (u32 i = 0; i < CTK_ARRAY_COUNT(mesh_infos); ++i)
        load_mesh(ctk_push(&assets->meshes, mesh_infos[i].name), mesh_infos[i].path, vk);
}

////////////////////////////////////////////////////////////
/// App
////////////////////////////////////////////////////////////
struct model_mtxs {
    glm::mat4 model;
    glm::mat4 mvp;
};

struct app {
    struct vtk_vertex_layout vertex_layout;
    struct {
        struct vtk_uniform_buffer entity_model_mtxs;
    } uniform_buffers;
    struct {
        struct vtk_image depth;
    } attachment_images;
    struct {
        VkDescriptorPool pool;
        struct {
            VkDescriptorSetLayout entity_model_mtxs;
        } set_layouts;
        struct {
            struct vtk_descriptor_set entity_model_mtxs;
        } sets;
    } descriptor;
    struct {
        struct vtk_render_pass main;
    } render_passes;
    struct {
        struct vtk_graphics_pipeline main;
    } graphics_pipelines;
    struct {
        VkSemaphore swapchain_img_aquired;
        VkSemaphore render_finished;
    } semaphores;
};

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
    vtk_validate_result(vkCreateDescriptorPool(vk->device.logical, &pool_info, NULL, &app->descriptor.pool),
                        "failed to create descriptor pool");

    // Layouts

    // entity_model_mtxs
    {
        VkDescriptorSetLayoutBinding binding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT };
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;
        vtk_validate_result(vkCreateDescriptorSetLayout(vk->device.logical, &info, NULL, &app->descriptor.set_layouts.entity_model_mtxs),
                            "error creating descriptor set layout");
    }

    // Sets

    // entity_model_mtxs
    vtk_allocate_descriptor_set(&app->descriptor.sets.entity_model_mtxs, app->descriptor.set_layouts.entity_model_mtxs, vk->swapchain.image_count,
                                vk->device.logical, app->descriptor.pool);
    ctk_push(&app->descriptor.sets.entity_model_mtxs.dynamic_offsets, app->uniform_buffers.entity_model_mtxs.element_size);

    // Updates
    struct ctk_array<VkDescriptorBufferInfo, 32> buf_infos = {};
    struct ctk_array<VkDescriptorImageInfo, 32> img_infos = {};
    struct ctk_array<VkWriteDescriptorSet, 32> writes = {};

    // entity_model_mtxs
    for (u32 i = 0; i < app->uniform_buffers.entity_model_mtxs.regions.count; ++i) {
        struct vtk_region *region = app->uniform_buffers.entity_model_mtxs.regions + i;
        VkDescriptorBufferInfo *info = ctk_push(&buf_infos);
        info->buffer = region->buffer->handle;
        info->offset = region->offset;
        info->range = region->size;

        VkWriteDescriptorSet *write = ctk_push(&writes);
        write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write->dstSet = app->descriptor.sets.entity_model_mtxs.instances[i];
        write->dstBinding = 0;
        write->dstArrayElement = 0;
        write->descriptorCount = 1;
        write->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        write->pBufferInfo = info;
    }

    vkUpdateDescriptorSets(vk->device.logical, writes.count, writes.data, 0, NULL);
}

static void create_render_passes(struct app *app, struct vk_core *vk) {
    // Main
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
        swapchain_attachment->finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
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
            ctk_push(&fb_info->attachments, app->attachment_images.depth.view);
            ctk_push(&fb_info->attachments, vk->swapchain.image_views[i]);
            fb_info->extent = vk->swapchain.extent;
            fb_info->layers = 1;
        }

        app->render_passes.main = vtk_create_render_pass(vk->device.logical, vk->graphics_cmd_pool, &rp_info);
    }
}

static void create_graphics_pipelines(struct app *app, struct vk_core *vk, struct assets *assets) {
    // Main
    {
        struct vtk_graphics_pipeline_info info = vtk_default_graphics_pipeline_info();
        ctk_push(&info.shaders, ctk_at(&assets->shaders, "barriers_shadow_vert"));
        ctk_push(&info.shaders, ctk_at(&assets->shaders, "barriers_shadow_frag"));
        ctk_push(&info.descriptor_set_layouts, app->descriptor.set_layouts.entity_model_mtxs);
        ctk_push(&info.vertex_inputs, { 0, 0, ctk_at(&app->vertex_layout.attributes, "position") });
        ctk_push(&info.vertex_input_binding_descriptions, { 0, app->vertex_layout.size, VK_VERTEX_INPUT_RATE_VERTEX });
        ctk_push(&info.viewports, { 0, 0, (f32)vk->swapchain.extent.width, (f32)vk->swapchain.extent.height, 0, 1 });
        ctk_push(&info.scissors, { 0, 0, vk->swapchain.extent.width, vk->swapchain.extent.height });
        ctk_push(&info.color_blend_attachment_states, vtk_default_color_blend_attachment_state());
        app->graphics_pipelines.main = vtk_create_graphics_pipeline(vk->device.logical, &app->render_passes.main, 0, &info);
    }
}

static void init_app(struct app *app, struct vk_core *vk, struct assets *assets) {
    // Vertex Layout
    vtk_push_vertex_attribute(&app->vertex_layout, "position", 3);
    vtk_push_vertex_attribute(&app->vertex_layout, "normal", 3);
    vtk_push_vertex_attribute(&app->vertex_layout, "uv", 2);

    // Uniform Buffers
    app->uniform_buffers.entity_model_mtxs = vtk_create_uniform_buffer(&vk->buffers.host, &vk->device, 1, sizeof(struct model_mtxs),
                                                                       vk->swapchain.image_count);

    // Attachment Images
    struct vtk_image_info depth_image_info = {};
    depth_image_info.width = vk->swapchain.extent.width;
    depth_image_info.height = vk->swapchain.extent.height;
    depth_image_info.format = vk->device.depth_image_format;
    depth_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    depth_image_info.usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_image_info.memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    depth_image_info.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
    app->attachment_images.depth = vtk_create_image(&vk->device, &depth_image_info);

    create_descriptor_sets(app, vk);
    create_render_passes(app, vk);
    create_graphics_pipelines(app, vk, assets);

    // Semaphores
    VkSemaphoreCreateInfo semaphore_ci = {};
    semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_ci.flags = 0;
    vtk_validate_result(vkCreateSemaphore(vk->device.logical, &semaphore_ci, NULL, &app->semaphores.swapchain_img_aquired),
                        "failed to create semaphore");
    vtk_validate_result(vkCreateSemaphore(vk->device.logical, &semaphore_ci, NULL, &app->semaphores.render_finished),
                        "failed to create semaphore");

    // // Fences
    // VkFenceCreateInfo fence_ci = {};
    // fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    // VkFence Fence = VK_NULL_HANDLE;
    // vtk_validate_result(vkCreateFence(vk->device.logical, &fence_ci, NULL, &app->fences.), "vkCreateFence", "failed to create fence");

}

static void record_render_passes(struct app *app, struct vk_core *vk, struct assets *assets, u32 swapchain_img_idx) {
    // Main
    {
        struct vtk_render_pass *rp = &app->render_passes.main;

        VkCommandBufferBeginInfo cmd_buf_begin_info = {};
        cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_begin_info.flags = 0;
        cmd_buf_begin_info.pInheritanceInfo = NULL;

        VkCommandBuffer cmd_buf = rp->command_buffers[swapchain_img_idx];
        vtk_validate_result(vkBeginCommandBuffer(cmd_buf, &cmd_buf_begin_info), "failed to begin recording command buffer");

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
            struct vtk_graphics_pipeline *gp = &app->graphics_pipelines.main;
            struct vtk_descriptor_set *desc_sets[] = { &app->descriptor.sets.entity_model_mtxs };
            struct mesh *mesh = ctk_at(&assets->meshes, "cube");

            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, gp->handle);
            vtk_bind_descriptor_sets(cmd_buf, gp->layout, desc_sets, CTK_ARRAY_COUNT(desc_sets), 0, swapchain_img_idx, 0);
            vkCmdBindVertexBuffers(cmd_buf, 0, 1, &mesh->vertex_region.buffer->handle, &mesh->vertex_region.offset);
            vkCmdBindIndexBuffer(cmd_buf, mesh->index_region.buffer->handle, mesh->index_region.offset, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd_buf, mesh->indexes.count, 1, 0, 0, 0);
        vkCmdEndRenderPass(cmd_buf);
        vtk_validate_result(vkEndCommandBuffer(cmd_buf), "error during render pass command recording");
    }
}

////////////////////////////////////////////////////////////
/// Scene
////////////////////////////////////////////////////////////
struct transform {
    struct ctk_v3 position;
    struct ctk_v3 rotation;
    struct ctk_v3 scale;
};

struct camera {
    struct transform transform;
    f32 fov;
    f32 aspect;
    f32 z_near;
    f32 z_far;
};

struct entity {
    struct transform *transform;
};

struct scene {
    static u32 const MAX_ENTITIES = 1024;
    struct camera camera;
    struct ctk_map<struct entity, MAX_ENTITIES> entities;
    struct {
        struct ctk_array<struct transform, MAX_ENTITIES> transforms;
        struct ctk_array<struct model_mtxs, MAX_ENTITIES> model_mtxs;
    } entity;
};

static struct transform default_transform() {
    return { {}, {}, { 1, 1, 1 } };
}

static struct entity *push_entity(struct scene *scene, cstr name) {
    ctk_push(&scene->entity.model_mtxs);
    return ctk_push(&scene->entities, name, {
        ctk_push(&scene->entity.transforms, default_transform()),
    });
}

static void init_scene(struct scene *scene, struct vk_core *vk) {
    scene->camera.transform = default_transform();
    scene->camera.transform.position = { 0, -1, -1 };
    scene->camera.fov = 90.0f;
    scene->camera.aspect = vk->swapchain.extent.width / (f32)vk->swapchain.extent.height;
    scene->camera.z_near = 0.01f;
    scene->camera.z_far = 50.0f;

    struct entity *cube = push_entity(scene, "cube");
    cube->transform->position = { 1, -1, 1 };
}

static glm::mat4 calculate_camera_mtx(struct camera* cam) {
    struct transform *cam_trans = &cam->transform;

    // View Matrix
    glm::vec3 cam_pos = { cam_trans->position.x, cam_trans->position.y, cam_trans->position.z };
    glm::mat4 cam_mtx(1.0f);
    cam_mtx = glm::rotate(cam_mtx, glm::radians(cam_trans->rotation.x), { 1.0f, 0.0f, 0.0f });
    cam_mtx = glm::rotate(cam_mtx, glm::radians(cam_trans->rotation.y), { 0.0f, 1.0f, 0.0f });
    cam_mtx = glm::rotate(cam_mtx, glm::radians(cam_trans->rotation.z), { 0.0f, 0.0f, 1.0f });
    cam_mtx = glm::translate(cam_mtx, cam_pos);
    glm::vec3 cam_forward = { cam_mtx[0][2], cam_mtx[1][2], cam_mtx[2][2] };
    glm::mat4 view_mtx = glm::lookAt(cam_pos, cam_pos + cam_forward, { 0.0f, -1.0f, 0.0f });

    // Projection Matrix
    glm::mat4 proj_mtx = glm::perspective(glm::radians(cam->fov), cam->aspect, cam->z_near, cam->z_far);
    proj_mtx[1][1] *= -1; // Flip y value for scale (glm is designed for OpenGL).

    return proj_mtx * view_mtx;
}

static void update_model_mtxs(glm::mat4 view_space_mtx, u32 model_count, struct transform *transforms, struct model_mtxs *model_mtxs_array) {
    if (model_count == 0)
        return;

    for (u32 i = 0; i < model_count; ++i) {
        struct transform *trans = transforms + i;
        struct model_mtxs *model_mtxs = model_mtxs_array + i;
        glm::mat4 model_mtx(1.0f);
        model_mtx = glm::translate(model_mtx, { trans->position.x, trans->position.y, trans->position.z });
        model_mtx = glm::rotate(model_mtx, glm::radians(trans->rotation.x), { 1.0f, 0.0f, 0.0f });
        model_mtx = glm::rotate(model_mtx, glm::radians(trans->rotation.y), { 0.0f, 1.0f, 0.0f });
        model_mtx = glm::rotate(model_mtx, glm::radians(trans->rotation.z), { 0.0f, 0.0f, 1.0f });
        model_mtx = glm::scale(model_mtx, { trans->scale.x, trans->scale.y, trans->scale.z });
        model_mtxs->model = model_mtx;
        model_mtxs->mvp = view_space_mtx * model_mtx;
    }
}

static void update_scene(struct scene *scene, struct vk_core *vk, struct app *app, u32 swapchain_img_idx) {
    glm::mat4 camera_mtx = calculate_camera_mtx(&scene->camera);
    update_model_mtxs(camera_mtx, scene->entities.count, scene->entity.transforms.data, scene->entity.model_mtxs.data);
    vtk_write_to_host_region(vk->device.logical, scene->entity.model_mtxs.data, ctk_byte_count(&scene->entity.model_mtxs),
                             app->uniform_buffers.entity_model_mtxs.regions + swapchain_img_idx, 0);
}

static void submit_command_buffers(struct app *app, struct vk_core *vk, u32 swapchain_img_idx) {
    // Render
    {
        VkCommandBuffer cmd_bufs[] = {
            app->render_passes.main.command_buffers[swapchain_img_idx],
        };
        VkSemaphore wait_semaphores[] = {
            app->semaphores.swapchain_img_aquired,
        };
        VkPipelineStageFlags wait_stages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkSemaphore signal_semaphores[] = {
            app->semaphores.render_finished,
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
        vtk_validate_result(vkQueueSubmit(vk->device.queues.graphics, 1, &submit_info, VK_NULL_HANDLE),
                            "failed to submit command buffer to graphics queue");
    }

    // Present
    {
        VkSemaphore wait_semaphores[] = {
            app->semaphores.render_finished,
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

////////////////////////////////////////////////////////////
/// Synchronization
////////////////////////////////////////////////////////////
static u32 vtk_aquire_swapchain_image_index(struct vk_core *vk, struct app *app) {
    // // Wait on current frame's fence if still unsignaled.
    // vkWaitForFences(logical_device, 1, &app->frame.in_flight_fences[app->frame.index], VK_TRUE, UINT64_MAX);

    // Aquire next swapchain image index, using a semaphore to signal when image is available for rendering.
    u32 swapchain_img_idx = VTK_UNSET_INDEX;
    vtk_validate_result(vkAcquireNextImageKHR(vk->device.logical, vk->swapchain.handle, UINT64_MAX, app->semaphores.swapchain_img_aquired,
                                              VK_NULL_HANDLE, &swapchain_img_idx),
                        "failed to aquire next swapchain image");

    return swapchain_img_idx;
}

////////////////////////////////////////////////////////////
/// Main
////////////////////////////////////////////////////////////
static void update_mouse_state(struct window *window) {
    // Mouse Delta
    struct ctk_v2d prev_mouse_position = window->mouse_position;
    f64 curr_mouse_x = 0.0;
    f64 curr_mouse_y = 0.0;
    glfwGetCursorPos(window->handle, &curr_mouse_x, &curr_mouse_y);
    window->mouse_position = { curr_mouse_x, curr_mouse_y };

    // Calculate delta if previous position was not unset.
    if (prev_mouse_position != UNSET_MOUSE_POSITION)
        window->mouse_delta = window->mouse_position - prev_mouse_position;
}

static void local_translate(struct transform *transform, struct ctk_v3 translation) {
    struct ctk_v3 *pos = &transform->position;
    struct ctk_v3 *rot = &transform->rotation;

    glm::mat4 world_mtx(1.0f);
    world_mtx = glm::rotate(world_mtx, glm::radians(rot->x), { 1.0f, 0.0f, 0.0f });
    world_mtx = glm::rotate(world_mtx, glm::radians(rot->y), { 0.0f, 1.0f, 0.0f });
    world_mtx = glm::rotate(world_mtx, glm::radians(rot->z), { 0.0f, 0.0f, 1.0f });
    world_mtx = glm::translate(world_mtx, { pos->x, pos->y, pos->z });

    struct ctk_v3 right = {};
    right.x = world_mtx[0][0];
    right.y = world_mtx[1][0];
    right.z = world_mtx[2][0];

    struct ctk_v3 up = {};
    up.x = world_mtx[0][1];
    up.y = world_mtx[1][1];
    up.z = world_mtx[2][1];

    struct ctk_v3 forward = {};
    forward.x = world_mtx[0][2];
    forward.y = world_mtx[1][2];
    forward.z = world_mtx[2][2];

    struct ctk_v3 new_pos = *pos;
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

    struct ctk_v3 translation = {};
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

void test_main() {
    struct window window = {};
    struct vk_core vk = {};
    struct assets assets = {};
    struct app app = {};
    struct scene scene = {};
    init_window(&window);
    init_vk_core(&vk, &window);
    load_assets(&assets, &vk);
    init_app(&app, &vk, &assets);
    init_scene(&scene, &vk);
    while (!glfwWindowShouldClose(window.handle)) {
        // Input
        glfwPollEvents();
        if (window.key_down[GLFW_KEY_ESCAPE])
            break;
        update_mouse_state(&window);
        camera_controls(&scene.camera.transform, &window);

        // Rendering
        u32 swapchain_img_idx = vtk_aquire_swapchain_image_index(&vk, &app);
        record_render_passes(&app, &vk, &assets, swapchain_img_idx);
        update_scene(&scene, &vk, &app, swapchain_img_idx);
        submit_command_buffers(&app, &vk, swapchain_img_idx);
        vkDeviceWaitIdle(vk.device.logical);

        // Sleep(1);
    }
}
