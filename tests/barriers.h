#include <windows.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vtk/vtk_new.h"

////////////////////////////////////////////////////////////
/// Window
////////////////////////////////////////////////////////////
struct window {
    GLFWwindow* handle;
    u32 width;
    u32 height;
};

static void error_callback(s32 err, cstr msg) {
    CTK_FATAL("[%d] %s", err, msg)
}

static void init_window(struct window *window) {
    glfwSetErrorCallback(error_callback);
    if (glfwInit() != GLFW_TRUE)
        CTK_FATAL("failed to init GLFW")
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window->width = 1600;
    window->height = 900;
    window->handle = glfwCreateWindow(window->width, window->height, "test", NULL, NULL);
    if (window->handle == NULL)
        CTK_FATAL("failed to create window")
    glfwSetWindowPos(window->handle, 320, 60);
    // glfwSetWindowUserPointer(window->Handle, info->user_pointer);
    // glfwSetKeyCallback(window->Handle, info->key_callback);
    // glfwSetMouseButtonCallback(window->Handle, info->mouse_button_callback);
}

////////////////////////////////////////////////////////////
/// Vulkan
////////////////////////////////////////////////////////////
struct vk {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR surface;
    struct device {
        VkPhysicalDevice physical;
        VkDevice logical;
        struct {
            u32 graphics;
            u32 present;
        } queue_family_indexes;
        struct {
            VkQueue graphics;
            VkQueue present;
        } queues;
    } device;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity_flag_bit,
                                                     VkDebugUtilsMessageTypeFlagsEXT msg_type_flags,
                                                     VkDebugUtilsMessengerCallbackDataEXT const *cb_data, void *user_data) {
    cstr msg_id = cb_data->pMessageIdName ? cb_data->pMessageIdName : "";
    if (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT & msg_severity_flag_bit)
        CTK_FATAL("VALIDATION LAYER [%s]: %s\n", msg_id, cb_data->pMessage)
    else if (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT & msg_severity_flag_bit)
        ctk::warning("VALIDATION LAYER [%s]: %s\n", msg_id, cb_data->pMessage);
    else
        ctk::info("VALIDATION LAYER [%s]: %s\n", msg_id, cb_data->pMessage);
    return VK_FALSE;
}

static VkDeviceQueueCreateInfo default_queue_create_info(u32 q_family_idx) {
    static f32 const Q_PRIORITIES[] = { 1.0f };

    VkDeviceQueueCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    ci.flags = 0;
    ci.queueFamilyIndex = q_family_idx;
    ci.queueCount = CTK_ARRAY_COUNT(Q_PRIORITIES);
    ci.pQueuePriorities = Q_PRIORITIES;

    return ci;
}

static void create_device(struct vk *vk, VkPhysicalDeviceFeatures *features) {
    ////////////////////////////////////////////////////////////
    /// Physical
    ////////////////////////////////////////////////////////////
    ctk::sarray<VkPhysicalDevice, 8> phys_devices = {};
    vkEnumeratePhysicalDevices(vk->instance, &phys_devices.Count, NULL);
    vkEnumeratePhysicalDevices(vk->instance, &phys_devices.Count, phys_devices.Data);
    vk->device.physical = phys_devices[0];

    // Find queue family indexes.
    ctk::sarray<VkQueueFamilyProperties, 8> q_family_props_arr = {};
    vkGetPhysicalDeviceQueueFamilyProperties(vk->device.physical, &q_family_props_arr.Count, NULL);
    vkGetPhysicalDeviceQueueFamilyProperties(vk->device.physical, &q_family_props_arr.Count, q_family_props_arr.Data);
    for(u32 i = 0; i < q_family_props_arr.Count; ++i) {
        VkQueueFamilyProperties *q_family_props = q_family_props_arr + i;
        if (q_family_props->queueFlags & VK_QUEUE_GRAPHICS_BIT)
            vk->device.queue_family_indexes.graphics = i;
        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(vk->device.physical, i, vk->surface, &present_supported);
        if (present_supported == VK_TRUE)
            vk->device.queue_family_indexes.present = i;
    }

    ////////////////////////////////////////////////////////////
    /// Logical
    ////////////////////////////////////////////////////////////
    ctk::sarray<VkDeviceQueueCreateInfo, 2> queue_cis = {};
    ctk::push(&queue_cis, default_queue_create_info(vk->device.queue_family_indexes.graphics));
    if (vk->device.queue_family_indexes.present != vk->device.queue_family_indexes.graphics)
        push(&queue_cis, default_queue_create_info(vk->device.queue_family_indexes.present));

    cstr extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo log_device_ci = {};
    log_device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    log_device_ci.flags = 0;
    log_device_ci.queueCreateInfoCount = queue_cis.Count;
    log_device_ci.pQueueCreateInfos = queue_cis.Data;
    log_device_ci.enabledLayerCount = 0;
    log_device_ci.ppEnabledLayerNames = NULL;
    log_device_ci.enabledExtensionCount = CTK_ARRAY_COUNT(extensions);
    log_device_ci.ppEnabledExtensionNames = extensions;
    log_device_ci.pEnabledFeatures = features;
    vtk_validate_vk_result(vkCreateDevice(vk->device.physical, &log_device_ci, NULL, &vk->device.logical),
                           "vkCreateDevice", "failed to create logical device");

    // Get logical device queues.
    static u32 const QUEUE_INDEX = 0; // Currently only supporting 1 queue per family.
    vkGetDeviceQueue(vk->device.logical, vk->device.queue_family_indexes.graphics, QUEUE_INDEX, &vk->device.queues.graphics);
    vkGetDeviceQueue(vk->device.logical, vk->device.queue_family_indexes.present, QUEUE_INDEX, &vk->device.queues.present);
}

static void init_vk(struct vk *vk, struct window *window) {
    ////////////////////////////////////////////////////////////
    /// Instance & Debug Messenger
    ////////////////////////////////////////////////////////////
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = NULL;
    app_info.pApplicationName = "test";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "test";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_ci = {};
    debug_messenger_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_ci.pNext = NULL;
    debug_messenger_ci.flags = 0;
    debug_messenger_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                         // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_ci.pfnUserCallback = debug_callback;
    debug_messenger_ci.pUserData = NULL;

    ctk::sarray<cstr, 8> extensions = {};
    ctk::sarray<cstr, 8> layers = {};
    cstr *glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions.Count);
    memcpy(extensions.Data, glfw_extensions, ctk::byte_count(&extensions));
    ctk::push(&extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    ctk::push(&layers, "VK_LAYER_LUNARG_standard_validation");

    VkInstanceCreateInfo instance_ci = {};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pNext = &debug_messenger_ci;
    instance_ci.flags = 0;
    instance_ci.pApplicationInfo = &app_info;
    instance_ci.enabledLayerCount = layers.Count;
    instance_ci.ppEnabledLayerNames = layers.Data;
    instance_ci.enabledExtensionCount = extensions.Count;
    instance_ci.ppEnabledExtensionNames = extensions.Data;
    vtk_validate_vk_result(vkCreateInstance(&instance_ci, NULL, &vk->instance), "vkCreateInstance", "failed to create Vulkan instance");

    VTK_LOAD_INSTANCE_EXTENSION_FUNCTION(vk->instance, vkCreateDebugUtilsMessengerEXT)
    vtk_validate_vk_result(vkCreateDebugUtilsMessengerEXT(vk->instance, &debug_messenger_ci, NULL, &vk->debug_messenger),
                           "vkCreateDebugUtilsMessengerEXT", "failed to create debug messenger");

    ////////////////////////////////////////////////////////////
    /// Surface
    ////////////////////////////////////////////////////////////
    vtk_validate_vk_result(glfwCreateWindowSurface(vk->instance, window->handle, NULL, &vk->surface),
                           "glfwCreateWindowSurface", "failed to create GLFW surface");

    ////////////////////////////////////////////////////////////
    /// Device
    ////////////////////////////////////////////////////////////
    VkPhysicalDeviceFeatures features = {};
    features.geometryShader = VK_TRUE;
    features.samplerAnisotropy = VK_TRUE;
    create_device(vk, &features);

    ////////////////////////////////////////////////////////////
    /// Swapchain
    ////////////////////////////////////////////////////////////
}

////////////////////////////////////////////////////////////
/// State
////////////////////////////////////////////////////////////
struct state {
};

////////////////////////////////////////////////////////////
/// Main
////////////////////////////////////////////////////////////
void test_main() {
    struct window window = {};
    struct vk vk = {};
    struct state state = {};
    init_window(&window);
    init_vk(&vk, &window);
    // init_state(&state, &vk);
    while (!glfwWindowShouldClose(window.handle)) {
        glfwPollEvents();
        Sleep(1);
    }
}
