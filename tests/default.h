#pragma once

static void
default_test_create_state(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState) {}

static scene *
default_test_create_scene(assets *Assets, vulkan_state *VulkanState)
{
    return create_scene(Assets, VulkanState, "assets/scenes/default.ctkd");
}

static void
default_test_init(vulkan_instance *VulkanInstance, assets *Assets, vulkan_state *VulkanState, scene *Scene)
{
    record_default_command_buffers(VulkanInstance, VulkanState, Scene);
}

static void
default_test_update(vulkan_instance *VulkanInstance, vulkan_state *VulkanState)
{
    render_default_render_pass(VulkanInstance, VulkanState);
}
