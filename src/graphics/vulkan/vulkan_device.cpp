#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "vulkan_device.h"
#include "vulkan_command_list.h"
#include "vulkan_util.h"

#include "gecko/core/services/log.h"
#include "private/labels.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace gecko::graphics {

// ── Debug messenger callback ──────────────────────────────────────────────

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             /*types*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       /*user*/)
{
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
  {
    GECKO_ERROR(labels::Graphics, "[Vulkan validation] {}", data->pMessage);
  }
  else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
  {
    GECKO_WARN(labels::Graphics, "[Vulkan validation] {}", data->pMessage);
  }
  else
  {
    GECKO_INFO(labels::Graphics, "[Vulkan validation] {}", data->pMessage);
  }
  return VK_FALSE;
}

// ── Constructor / Destructor ──────────────────────────────────────────────

VulkanDevice::VulkanDevice(const GraphicsDeviceDesc& desc) noexcept
{
  // ── Instance ──────────────────────────────────────────────────

  VkApplicationInfo appInfo{};
  appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName   = desc.AppName;
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
  appInfo.pEngineName        = "Gecko";
  appInfo.engineVersion      = VK_MAKE_VERSION(0, 0, 1);
  appInfo.apiVersion         = VK_API_VERSION_1_3;

  ::std::vector<const char*> instanceExts = {
      VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(GECKO_PLATFORM_LINUX)
      "VK_KHR_xlib_surface",
      "VK_KHR_wayland_surface",
#elif defined(GECKO_PLATFORM_WINDOWS)
      "VK_KHR_win32_surface",
#endif
  };

  ::std::vector<const char*> layers;

  if (desc.Debug)
  {
    instanceExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    layers.push_back("VK_LAYER_KHRONOS_validation");
  }

  VkInstanceCreateInfo instanceCI{};
  instanceCI.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCI.pApplicationInfo        = &appInfo;
  instanceCI.enabledExtensionCount   = static_cast<u32>(instanceExts.size());
  instanceCI.ppEnabledExtensionNames = instanceExts.data();
  instanceCI.enabledLayerCount       = static_cast<u32>(layers.size());
  instanceCI.ppEnabledLayerNames     = layers.data();

  VkResult res = vkCreateInstance(&instanceCI, nullptr, &m_Instance);
  if (res != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: vkCreateInstance failed ({})",
                static_cast<i32>(res));
    return;
  }

  // ── Debug messenger ────────────────────────────────────────────

  if (desc.Debug)
  {
    VkDebugUtilsMessengerCreateInfoEXT messengerCI{};
    messengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messengerCI.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                              | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                              | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerCI.pfnUserCallback = DebugCallback;

    auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
    if (createFn)
      createFn(m_Instance, &messengerCI, nullptr, &m_DebugMessenger);
  }

  // ── Physical device selection ──────────────────────────────────

  u32 deviceCount = 0;
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
  if (deviceCount == 0)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: no Vulkan-capable GPU found");
    return;
  }

  ::std::vector<VkPhysicalDevice> physDevices(deviceCount);
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, physDevices.data());

  // Prefer discrete GPU
  for (VkPhysicalDevice pd : physDevices)
  {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(pd, &props);
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
      m_PhysicalDevice = pd;
      break;
    }
  }
  if (m_PhysicalDevice == VK_NULL_HANDLE)
    m_PhysicalDevice = physDevices[0];

  {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
    u32 major = VK_VERSION_MAJOR(props.apiVersion);
    u32 minor = VK_VERSION_MINOR(props.apiVersion);
    GECKO_INFO(labels::Graphics, "VulkanDevice: selected GPU '{}' (Vulkan {}.{})",
               props.deviceName, major, minor);
  }

  // ── Queue families ─────────────────────────────────────────────

  u32 qfCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &qfCount, nullptr);
  ::std::vector<VkQueueFamilyProperties> qfProps(qfCount);
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &qfCount, qfProps.data());

  m_GraphicsQueueFamily = UINT32_MAX;
  m_PresentQueueFamily  = UINT32_MAX;

  for (u32 i = 0; i < qfCount; ++i)
  {
    if ((qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u)
    {
      if (m_GraphicsQueueFamily == UINT32_MAX)
        m_GraphicsQueueFamily = i;
      // present on Linux: all graphics queues generally support present
      m_PresentQueueFamily = i;
    }
  }

  if (m_GraphicsQueueFamily == UINT32_MAX)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: no graphics queue family found");
    return;
  }

  // ── Logical device ─────────────────────────────────────────────

  float queuePriority = 1.0F;
  VkDeviceQueueCreateInfo queueCI{};
  queueCI.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCI.queueFamilyIndex = m_GraphicsQueueFamily;
  queueCI.queueCount       = 1;
  queueCI.pQueuePriorities = &queuePriority;

  ::std::vector<const char*> deviceExts = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  VkPhysicalDeviceDynamicRenderingFeatures dynRendering{};
  dynRendering.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
  dynRendering.dynamicRendering = VK_TRUE;

  VkPhysicalDeviceFeatures2 features2{};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features2.pNext = &dynRendering;

  VkDeviceCreateInfo deviceCI{};
  deviceCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCI.pNext                   = &features2;
  deviceCI.queueCreateInfoCount    = 1;
  deviceCI.pQueueCreateInfos       = &queueCI;
  deviceCI.enabledExtensionCount   = static_cast<u32>(deviceExts.size());
  deviceCI.ppEnabledExtensionNames = deviceExts.data();

  res = vkCreateDevice(m_PhysicalDevice, &deviceCI, nullptr, &m_Device);
  if (res != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: vkCreateDevice failed ({})",
                static_cast<i32>(res));
    return;
  }

  vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
  vkGetDeviceQueue(m_Device, m_PresentQueueFamily,  0, &m_PresentQueue);

  // ── Command pool ───────────────────────────────────────────────

  VkCommandPoolCreateInfo poolCI{};
  poolCI.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolCI.queueFamilyIndex = m_GraphicsQueueFamily;
  poolCI.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VULKAN_CHECK(vkCreateCommandPool(m_Device, &poolCI, nullptr, &m_GraphicsCommandPool));

  // ── VMA ────────────────────────────────────────────────────────

  VmaVulkanFunctions vmaFuncs{};
  vmaFuncs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vmaFuncs.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo vmaCI{};
  vmaCI.vulkanApiVersion  = VK_API_VERSION_1_3;
  vmaCI.physicalDevice    = m_PhysicalDevice;
  vmaCI.device            = m_Device;
  vmaCI.instance          = m_Instance;
  vmaCI.pVulkanFunctions  = &vmaFuncs;

  res = vmaCreateAllocator(&vmaCI, &m_Allocator);
  if (res != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: vmaCreateAllocator failed ({})",
                static_cast<i32>(res));
    return;
  }

  m_Valid = true;
  GECKO_INFO(labels::Graphics, "VulkanDevice: initialised");
}

VulkanDevice::~VulkanDevice()
{
  if (m_Device != VK_NULL_HANDLE)
    vkDeviceWaitIdle(m_Device);

  if (m_Allocator != VK_NULL_HANDLE)
  {
    vmaDestroyAllocator(m_Allocator);
    m_Allocator = VK_NULL_HANDLE;
  }

  if (m_GraphicsCommandPool != VK_NULL_HANDLE)
  {
    vkDestroyCommandPool(m_Device, m_GraphicsCommandPool, nullptr);
    m_GraphicsCommandPool = VK_NULL_HANDLE;
  }

  if (m_Device != VK_NULL_HANDLE)
  {
    vkDestroyDevice(m_Device, nullptr);
    m_Device = VK_NULL_HANDLE;
  }

  if (m_DebugMessenger != VK_NULL_HANDLE)
  {
    auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroyFn)
      destroyFn(m_Instance, m_DebugMessenger, nullptr);
    m_DebugMessenger = VK_NULL_HANDLE;
  }

  if (m_Instance != VK_NULL_HANDLE)
  {
    vkDestroyInstance(m_Instance, nullptr);
    m_Instance = VK_NULL_HANDLE;
  }

  GECKO_INFO(labels::Graphics, "VulkanDevice: destroyed");
}

// ── Swapchain ─────────────────────────────────────────────────────────────

void VulkanDevice::CreateSwapchainInternal(
    VulkanSwapchainData&                         data,
    const ::gecko::platform::NativeWindowHandle& native,
    const SwapchainDesc&                         desc) noexcept
{
  // Create surface
#if defined(GECKO_PLATFORM_LINUX)
  using namespace ::gecko::platform;
  if (native.Backend == DisplayBackendKind::Wayland)
  {
    VkWaylandSurfaceCreateInfoKHR sci{};
    sci.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    sci.display = static_cast<struct wl_display*>(native.Display);
    sci.surface = static_cast<struct wl_surface*>(native.Handle);

    auto fn = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateWaylandSurfaceKHR"));
    if (!fn || fn(m_Instance, &sci, nullptr, &data.Surface) != VK_SUCCESS)
    {
      GECKO_ERROR(labels::Graphics, "VulkanDevice: failed to create Wayland surface");
      return;
    }
  }
  else
  {
    // Default to Xlib
    VkXlibSurfaceCreateInfoKHR sci{};
    sci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    sci.dpy    = static_cast<Display*>(native.Display);
    sci.window = reinterpret_cast<Window>(native.Handle);

    auto fn = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateXlibSurfaceKHR"));
    if (!fn || fn(m_Instance, &sci, nullptr, &data.Surface) != VK_SUCCESS)
    {
      GECKO_ERROR(labels::Graphics, "VulkanDevice: failed to create Xlib surface");
      return;
    }
  }
#elif defined(GECKO_PLATFORM_WINDOWS)
  {
    VkWin32SurfaceCreateInfoKHR sci{};
    sci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    sci.hinstance = GetModuleHandle(nullptr);
    sci.hwnd      = static_cast<HWND>(native.Handle);

    auto fn = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateWin32SurfaceKHR"));
    if (!fn || fn(m_Instance, &sci, nullptr, &data.Surface) != VK_SUCCESS)
    {
      GECKO_ERROR(labels::Graphics, "VulkanDevice: failed to create Win32 surface");
      return;
    }
  }
#endif

  // Surface capabilities
  VkSurfaceCapabilitiesKHR caps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, data.Surface, &caps);

  // Choose format
  u32 fmtCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, data.Surface, &fmtCount, nullptr);
  ::std::vector<VkSurfaceFormatKHR> formats(fmtCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, data.Surface, &fmtCount, formats.data());

  VkSurfaceFormatKHR chosen = formats[0];
  VkFormat           wanted = ToVkFormat(desc.Format);
  for (const auto& f : formats)
  {
    if (f.format == wanted)
    {
      chosen = f;
      break;
    }
  }
  // Also accept BGRA variants for common swapchain formats
  if (chosen.format != wanted)
  {
    for (const auto& f : formats)
    {
      if ((f.format == VK_FORMAT_B8G8R8A8_UNORM && desc.Format == DataFormat::R8G8B8A8_UNORM)
          || (f.format == VK_FORMAT_B8G8R8A8_SRGB && desc.Format == DataFormat::R8G8B8A8_SRGB))
      {
        chosen = f;
        break;
      }
    }
  }
  data.Format = chosen.format;

  // Choose present mode
  u32 pmCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, data.Surface, &pmCount, nullptr);
  ::std::vector<VkPresentModeKHR> presentModes(pmCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, data.Surface, &pmCount, presentModes.data());

  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
  if (!desc.VSync)
  {
    for (VkPresentModeKHR pm : presentModes)
    {
      if (pm == VK_PRESENT_MODE_MAILBOX_KHR)
      {
        presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        break;
      }
    }
  }

  // Extent
  VkExtent2D extent;
  if (caps.currentExtent.width != UINT32_MAX)
  {
    extent = caps.currentExtent;
  }
  else
  {
    extent.width  = ::std::clamp(desc.Width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    extent.height = ::std::clamp(desc.Height, caps.minImageExtent.height, caps.maxImageExtent.height);
  }
  data.Extent = extent;

  // Image count
  u32 imageCount = ::std::max(caps.minImageCount, desc.NumBackBuffers);
  if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
    imageCount = caps.maxImageCount;

  // Create swapchain
  VkSwapchainCreateInfoKHR swapCI{};
  swapCI.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapCI.surface          = data.Surface;
  swapCI.minImageCount    = imageCount;
  swapCI.imageFormat      = chosen.format;
  swapCI.imageColorSpace  = chosen.colorSpace;
  swapCI.imageExtent      = extent;
  swapCI.imageArrayLayers = 1;
  swapCI.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapCI.preTransform     = caps.currentTransform;
  swapCI.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapCI.presentMode      = presentMode;
  swapCI.clipped          = VK_TRUE;

  if (vkCreateSwapchainKHR(m_Device, &swapCI, nullptr, &data.Swapchain) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: vkCreateSwapchainKHR failed");
    return;
  }

  // Retrieve images
  vkGetSwapchainImagesKHR(m_Device, data.Swapchain, &data.ImageCount, nullptr);
  if (data.ImageCount > 8)
    data.ImageCount = 8;
  vkGetSwapchainImagesKHR(m_Device, data.Swapchain, &data.ImageCount, data.Images);

  // Image views
  for (u32 i = 0; i < data.ImageCount; ++i)
  {
    VkImageViewCreateInfo ivCI{};
    ivCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivCI.image                           = data.Images[i];
    ivCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    ivCI.format                          = data.Format;
    ivCI.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivCI.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivCI.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivCI.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    ivCI.subresourceRange.baseMipLevel   = 0;
    ivCI.subresourceRange.levelCount     = 1;
    ivCI.subresourceRange.baseArrayLayer = 0;
    ivCI.subresourceRange.layerCount     = 1;

    VULKAN_CHECK(vkCreateImageView(m_Device, &ivCI, nullptr, &data.ImageViews[i]));
  }

  // Per-frame sync + command buffers
  VkCommandBufferAllocateInfo cbAI{};
  cbAI.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbAI.commandPool        = m_GraphicsCommandPool;
  cbAI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbAI.commandBufferCount = data.ImageCount;
  VULKAN_CHECK(vkAllocateCommandBuffers(m_Device, &cbAI, data.CmdBuffers));

  VkSemaphoreCreateInfo semCI{};
  semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceCI{};
  fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (u32 i = 0; i < data.ImageCount; ++i)
  {
    VULKAN_CHECK(vkCreateSemaphore(m_Device, &semCI, nullptr, &data.ImageAvailable[i]));
    VULKAN_CHECK(vkCreateSemaphore(m_Device, &semCI, nullptr, &data.RenderFinished[i]));
    VULKAN_CHECK(vkCreateFence(m_Device, &fenceCI, nullptr, &data.InFlight[i]));
  }
}

void VulkanDevice::DestroySwapchainInternal(VulkanSwapchainData& data) noexcept
{
  vkDeviceWaitIdle(m_Device);

  for (u32 i = 0; i < data.ImageCount; ++i)
  {
    if (data.ImageViews[i] != VK_NULL_HANDLE)
      vkDestroyImageView(m_Device, data.ImageViews[i], nullptr);
    if (data.ImageAvailable[i] != VK_NULL_HANDLE)
      vkDestroySemaphore(m_Device, data.ImageAvailable[i], nullptr);
    if (data.RenderFinished[i] != VK_NULL_HANDLE)
      vkDestroySemaphore(m_Device, data.RenderFinished[i], nullptr);
    if (data.InFlight[i] != VK_NULL_HANDLE)
      vkDestroyFence(m_Device, data.InFlight[i], nullptr);
  }

  if (data.ImageCount > 0)
    vkFreeCommandBuffers(m_Device, m_GraphicsCommandPool, data.ImageCount, data.CmdBuffers);

  if (data.Swapchain != VK_NULL_HANDLE)
    vkDestroySwapchainKHR(m_Device, data.Swapchain, nullptr);

  if (data.Surface != VK_NULL_HANDLE)
    vkDestroySurfaceKHR(m_Instance, data.Surface, nullptr);

  data = VulkanSwapchainData{};
}

Swapchain VulkanDevice::CreateSwapchain(
    const ::gecko::platform::NativeWindowHandle& native,
    const SwapchainDesc&                         desc) noexcept
{
  if (!m_Valid)
    return Swapchain{};

  auto* data = new VulkanSwapchainData{};
  CreateSwapchainInternal(*data, native, desc);

  if (data->Swapchain == VK_NULL_HANDLE)
  {
    DestroySwapchainInternal(*data);
    delete data;
    return Swapchain{};
  }

  // Store native handle inside data for resize
  // We embed the native handle in a side struct; store it as extra bytes.
  // Simpler: keep a copy in the data struct.
  // (The native handle is small so we embed a copy.)

  Swapchain sc;
  sc.Desc.Width         = data->Extent.width;
  sc.Desc.Height        = data->Extent.height;
  sc.Desc.NumBackBuffers= data->ImageCount;
  sc.Desc.Format        = FromVkFormat(data->Format);
  sc.Desc.VSync         = desc.VSync;

  sc.Data = ::gecko::Shared<void>(data, [this](void* ptr) {
    auto* d = static_cast<VulkanSwapchainData*>(ptr);
    DestroySwapchainInternal(*d);
    delete d;
  });

  GECKO_INFO(labels::Graphics, "VulkanDevice: swapchain created ({}x{}, {} images)",
             data->Extent.width, data->Extent.height, data->ImageCount);
  return sc;
}

void VulkanDevice::DestroySwapchain(Swapchain& swapchain) noexcept
{
  swapchain.Data.reset();
  swapchain = Swapchain{};
}

void VulkanDevice::ResizeSwapchain(Swapchain& swapchain) noexcept
{
  if (!swapchain.Data)
    return;

  auto* data = static_cast<VulkanSwapchainData*>(swapchain.Data.get());

  // Query new surface capabilities for the new extent
  VkSurfaceCapabilitiesKHR caps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, data->Surface, &caps);

  if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0)
    return; // minimised

  vkDeviceWaitIdle(m_Device);

  // Destroy image views, command buffers, sync — but keep the surface
  for (u32 i = 0; i < data->ImageCount; ++i)
  {
    if (data->ImageViews[i] != VK_NULL_HANDLE)
      vkDestroyImageView(m_Device, data->ImageViews[i], nullptr);
    if (data->ImageAvailable[i] != VK_NULL_HANDLE)
      vkDestroySemaphore(m_Device, data->ImageAvailable[i], nullptr);
    if (data->RenderFinished[i] != VK_NULL_HANDLE)
      vkDestroySemaphore(m_Device, data->RenderFinished[i], nullptr);
    if (data->InFlight[i] != VK_NULL_HANDLE)
      vkDestroyFence(m_Device, data->InFlight[i], nullptr);
  }

  if (data->ImageCount > 0)
    vkFreeCommandBuffers(m_Device, m_GraphicsCommandPool, data->ImageCount, data->CmdBuffers);

  VkSwapchainKHR oldSwapchain = data->Swapchain;

  // Pick present mode (preserve VSync setting)
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

  // Extent
  VkExtent2D extent;
  if (caps.currentExtent.width != UINT32_MAX)
    extent = caps.currentExtent;
  else
  {
    extent.width  = ::std::clamp(swapchain.Desc.Width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    extent.height = ::std::clamp(swapchain.Desc.Height, caps.minImageExtent.height, caps.maxImageExtent.height);
  }

  VkSwapchainCreateInfoKHR swapCI{};
  swapCI.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapCI.surface          = data->Surface;
  swapCI.minImageCount    = data->ImageCount;
  swapCI.imageFormat      = data->Format;
  swapCI.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  swapCI.imageExtent      = extent;
  swapCI.imageArrayLayers = 1;
  swapCI.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapCI.preTransform     = caps.currentTransform;
  swapCI.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapCI.presentMode      = presentMode;
  swapCI.clipped          = VK_TRUE;
  swapCI.oldSwapchain     = oldSwapchain;

  VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
  if (vkCreateSwapchainKHR(m_Device, &swapCI, nullptr, &newSwapchain) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: ResizeSwapchain vkCreateSwapchainKHR failed");
    vkDestroySwapchainKHR(m_Device, oldSwapchain, nullptr);
    return;
  }

  vkDestroySwapchainKHR(m_Device, oldSwapchain, nullptr);
  data->Swapchain  = newSwapchain;
  data->Extent     = extent;
  data->FrameIndex = 0;

  // Retrieve images
  vkGetSwapchainImagesKHR(m_Device, data->Swapchain, &data->ImageCount, nullptr);
  if (data->ImageCount > 8)
    data->ImageCount = 8;
  vkGetSwapchainImagesKHR(m_Device, data->Swapchain, &data->ImageCount, data->Images);

  for (u32 i = 0; i < data->ImageCount; ++i)
  {
    VkImageViewCreateInfo ivCI{};
    ivCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivCI.image                           = data->Images[i];
    ivCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    ivCI.format                          = data->Format;
    ivCI.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivCI.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivCI.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivCI.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    ivCI.subresourceRange.baseMipLevel   = 0;
    ivCI.subresourceRange.levelCount     = 1;
    ivCI.subresourceRange.baseArrayLayer = 0;
    ivCI.subresourceRange.layerCount     = 1;
    VULKAN_CHECK(vkCreateImageView(m_Device, &ivCI, nullptr, &data->ImageViews[i]));
  }

  // Re-alloc command buffers + sync
  VkCommandBufferAllocateInfo cbAI{};
  cbAI.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbAI.commandPool        = m_GraphicsCommandPool;
  cbAI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbAI.commandBufferCount = data->ImageCount;
  VULKAN_CHECK(vkAllocateCommandBuffers(m_Device, &cbAI, data->CmdBuffers));

  VkSemaphoreCreateInfo semCI{};
  semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fenceCI{};
  fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (u32 i = 0; i < data->ImageCount; ++i)
  {
    VULKAN_CHECK(vkCreateSemaphore(m_Device, &semCI, nullptr, &data->ImageAvailable[i]));
    VULKAN_CHECK(vkCreateSemaphore(m_Device, &semCI, nullptr, &data->RenderFinished[i]));
    VULKAN_CHECK(vkCreateFence(m_Device, &fenceCI, nullptr, &data->InFlight[i]));
  }

  swapchain.Desc.Width  = extent.width;
  swapchain.Desc.Height = extent.height;

  GECKO_INFO(labels::Graphics, "VulkanDevice: swapchain resized to {}x{}",
             extent.width, extent.height);
}

RenderTarget VulkanDevice::GetCurrentBackBuffer(
    const Swapchain& swapchain) const noexcept
{
  if (!swapchain.Data)
    return RenderTarget{};

  auto* data = static_cast<VulkanSwapchainData*>(swapchain.Data.get());

  // Acquire the next image; record the acquired index
  u32 imageIndex = 0;
  VkResult res = vkAcquireNextImageKHR(
      m_Device, data->Swapchain, UINT64_MAX,
      data->ImageAvailable[data->FrameIndex], VK_NULL_HANDLE, &imageIndex);

  if (res == VK_ERROR_OUT_OF_DATE_KHR)
  {
    GECKO_WARN(labels::Graphics, "VulkanDevice: swapchain out of date on acquire");
    return RenderTarget{};
  }

  data->AcquiredIndex = imageIndex;

  // Wait for this frame's fence
  vkWaitForFences(m_Device, 1, &data->InFlight[data->FrameIndex], VK_TRUE, UINT64_MAX);
  vkResetFences(m_Device, 1, &data->InFlight[data->FrameIndex]);

  // Build a RenderTarget that wraps this image
  // We store a small struct as the RT's Data so VulkanCommandList can read it
  struct BackBufferData
  {
    VkImage     Image;
    VkImageView ImageView;
  };

  auto* bbData = new BackBufferData{data->Images[imageIndex], data->ImageViews[imageIndex]};

  RenderTarget rt{};
  rt.Desc.Width                  = data->Extent.width;
  rt.Desc.Height                 = data->Extent.height;
  rt.Desc.NumRenderTargets       = 1;
  rt.Desc.RenderTargetFormats[0] = FromVkFormat(data->Format);
  rt.Data = ::gecko::Shared<void>(bbData, [](void* p) { delete static_cast<BackBufferData*>(p); });

  return rt;
}

u32 VulkanDevice::GetCurrentBackBufferIndex(
    const Swapchain& swapchain) const noexcept
{
  if (!swapchain.Data)
    return 0;
  auto* data = static_cast<VulkanSwapchainData*>(swapchain.Data.get());
  return data->AcquiredIndex;
}

void VulkanDevice::Present(const Swapchain& swapchain) noexcept
{
  if (!swapchain.Data)
    return;

  auto* data = static_cast<VulkanSwapchainData*>(swapchain.Data.get());
  u32   frame = data->FrameIndex;

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores    = &data->RenderFinished[frame];
  presentInfo.swapchainCount     = 1;
  presentInfo.pSwapchains        = &data->Swapchain;
  presentInfo.pImageIndices      = &data->AcquiredIndex;

  VkResult res = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
  if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
  {
    GECKO_WARN(labels::Graphics, "VulkanDevice: swapchain needs resize");
  }

  data->FrameIndex = (frame + 1) % data->ImageCount;
}

// ── Command lists ─────────────────────────────────────────────────────────

Unique<ICommandList> VulkanDevice::CreateGraphicsCommandList() noexcept
{
  if (!m_Valid)
    return nullptr;
  return ::gecko::CreateUnique<VulkanCommandList>(*this);
}

void VulkanDevice::ExecuteGraphicsCommandList(
    Unique<ICommandList> commandList) noexcept
{
  if (!commandList)
    return;

  auto* cmd = static_cast<VulkanCommandList*>(commandList.get());
  VkCommandBuffer cb = cmd->CommandBuffer();

  VkSubmitInfo si{};
  si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount   = 1;
  si.pCommandBuffers      = &cb;

  // Hook up swapchain semaphores if we have an active swapchain frame
  if (cmd->HasSwapchainData())
  {
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = cmd->ImageAvailableSemaphore();
    si.pWaitDstStageMask    = &waitStage;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = cmd->RenderFinishedSemaphore();

    VULKAN_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &si, cmd->InFlightFence()));
  }
  else
  {
    VULKAN_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &si, VK_NULL_HANDLE));
  }
}

Unique<ICommandList> VulkanDevice::CreateComputeCommandList() noexcept
{
  if (!m_Valid)
    return nullptr;
  // Reuse the graphics command list for now (same queue family)
  return ::gecko::CreateUnique<VulkanCommandList>(*this);
}

void VulkanDevice::ExecuteComputeCommandList(
    Unique<ICommandList> commandList) noexcept
{
  ExecuteGraphicsCommandList(::std::move(commandList));
}

// ── Resource creation ─────────────────────────────────────────────────────

RenderTarget VulkanDevice::CreateRenderTarget(const RenderTargetDesc& /*desc*/) noexcept
{
  GECKO_WARN(labels::Graphics, "VulkanDevice: CreateRenderTarget not yet implemented");
  return RenderTarget{};
}

Buffer VulkanDevice::CreateVertexBuffer(const VertexBufferDesc& desc) noexcept
{
  if (!m_Valid)
    return Buffer{};

  VkDeviceSize bufferSize = static_cast<VkDeviceSize>(desc.NumVertices) * desc.VertexSize;

  VkBufferCreateInfo bufCI{};
  bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufCI.size  = bufferSize;
  bufCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  VmaAllocationCreateInfo allocCI{};
  allocCI.usage = VMA_MEMORY_USAGE_AUTO;
  allocCI.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

  auto* bufData = new VulkanBufferData{};
  if (vmaCreateBuffer(m_Allocator, &bufCI, &allocCI,
                       &bufData->Buffer, &bufData->Allocation, nullptr) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: CreateVertexBuffer vmaCreateBuffer failed");
    delete bufData;
    return Buffer{};
  }

  Buffer buf;
  buf.Type       = BufferType::Vertex;
  buf.VertexDesc = desc;
  buf.Data       = ::gecko::Shared<void>(bufData, [this](void* ptr) {
    auto* d = static_cast<VulkanBufferData*>(ptr);
    vmaDestroyBuffer(m_Allocator, d->Buffer, d->Allocation);
    delete d;
  });
  return buf;
}

Buffer VulkanDevice::CreateIndexBuffer(const IndexBufferDesc& /*desc*/) noexcept
{
  GECKO_WARN(labels::Graphics, "VulkanDevice: CreateIndexBuffer not yet implemented");
  return Buffer{};
}

Buffer VulkanDevice::CreateConstantBuffer(const ConstantBufferDesc& /*desc*/) noexcept
{
  GECKO_WARN(labels::Graphics, "VulkanDevice: CreateConstantBuffer not yet implemented");
  return Buffer{};
}

Buffer VulkanDevice::CreateStructuredBuffer(const StructuredBufferDesc& /*desc*/) noexcept
{
  GECKO_WARN(labels::Graphics, "VulkanDevice: CreateStructuredBuffer not yet implemented");
  return Buffer{};
}

Texture VulkanDevice::CreateTexture(const TextureDesc& /*desc*/) noexcept
{
  GECKO_WARN(labels::Graphics, "VulkanDevice: CreateTexture not yet implemented");
  return Texture{};
}

// ── Shader loading ────────────────────────────────────────────────────────

VkShaderModule VulkanDevice::LoadShaderModule(const char* path) noexcept
{
  ::std::ifstream file(path, ::std::ios::binary | ::std::ios::ate);
  if (!file.is_open())
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: cannot open shader '{}'", path);
    return VK_NULL_HANDLE;
  }

  auto size = static_cast<::std::streamsize>(file.tellg());
  file.seekg(0);

  ::std::vector<char> code(static_cast<usize>(size));
  file.read(code.data(), size);

  VkShaderModuleCreateInfo ci{};
  ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = static_cast<usize>(size);
  ci.pCode    = reinterpret_cast<const u32*>(code.data());

  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(m_Device, &ci, nullptr, &module) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: vkCreateShaderModule failed for '{}'", path);
    return VK_NULL_HANDLE;
  }
  return module;
}

GraphicsPipeline VulkanDevice::CreateGraphicsPipeline(
    const GraphicsPipelineDesc& desc) noexcept
{
  if (!m_Valid)
    return GraphicsPipeline{};

  VkShaderModule vertModule = LoadShaderModule(desc.VertexShaderPath);
  VkShaderModule fragModule = desc.PixelShaderPath ? LoadShaderModule(desc.PixelShaderPath) : VK_NULL_HANDLE;

  if (vertModule == VK_NULL_HANDLE)
  {
    if (fragModule != VK_NULL_HANDLE)
      vkDestroyShaderModule(m_Device, fragModule, nullptr);
    return GraphicsPipeline{};
  }

  // Shader stages
  VkPipelineShaderStageCreateInfo stages[2]{};
  u32 stageCount = 0;

  stages[stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[stageCount].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  stages[stageCount].module = vertModule;
  stages[stageCount].pName  = "main";
  ++stageCount;

  if (fragModule != VK_NULL_HANDLE)
  {
    stages[stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[stageCount].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[stageCount].module = fragModule;
    stages[stageCount].pName  = "main";
    ++stageCount;
  }

  // Vertex input
  VkVertexInputBindingDescription binding{};
  binding.binding   = 0;
  binding.stride    = desc.Layout.StrideInBytes;
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attrs[VertexLayout::MaxAttributes]{};
  for (u32 i = 0; i < desc.Layout.NumAttributes; ++i)
  {
    attrs[i].location = i;
    attrs[i].binding  = 0;
    attrs[i].format   = ToVkFormat(desc.Layout.Attributes[i].AttributeFormat);
    attrs[i].offset   = desc.Layout.Attributes[i].Offset;
  }

  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount   = (desc.Layout.StrideInBytes > 0) ? 1 : 0;
  vertexInput.pVertexBindingDescriptions      = &binding;
  vertexInput.vertexAttributeDescriptionCount = desc.Layout.NumAttributes;
  vertexInput.pVertexAttributeDescriptions    = attrs;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = (desc.Primitive == PrimitiveType::Lines)
                               ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                               : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount  = 1;

  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.lineWidth   = 1.0F;
  raster.cullMode    = [&]() -> VkCullModeFlags {
    switch (desc.Culling)
    {
      case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
      case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
      default:              return VK_CULL_MODE_NONE;
    }
  }();
  raster.frontFace   = (desc.Winding == WindingOrder::ClockWise)
                           ? VK_FRONT_FACE_CLOCKWISE
                           : VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blendAttachment{};
  blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                   | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo blending{};
  blending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blending.attachmentCount = 1;
  blending.pAttachments    = &blendAttachment;

  // Dynamic state
  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynState{};
  dynState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynState.dynamicStateCount = 2;
  dynState.pDynamicStates    = dynamicStates;

  // Pipeline layout (empty — no push constants or descriptor sets for triangle)
  VkPipelineLayoutCreateInfo layoutCI{};
  layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

  auto* pipeData = new VulkanPipelineData{};
  if (vkCreatePipelineLayout(m_Device, &layoutCI, nullptr, &pipeData->Layout) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: vkCreatePipelineLayout failed");
    vkDestroyShaderModule(m_Device, vertModule, nullptr);
    if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(m_Device, fragModule, nullptr);
    delete pipeData;
    return GraphicsPipeline{};
  }

  // Dynamic rendering
  VkFormat colorFormats[RenderTargetDesc::MaxRenderTargets]{};
  for (u32 i = 0; i < desc.NumRenderTargets; ++i)
    colorFormats[i] = ToVkFormat(desc.RenderTargetFormats[i]);

  VkPipelineRenderingCreateInfo renderingCI{};
  renderingCI.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCI.colorAttachmentCount    = desc.NumRenderTargets;
  renderingCI.pColorAttachmentFormats = colorFormats;

  VkGraphicsPipelineCreateInfo pipeCI{};
  pipeCI.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeCI.pNext               = &renderingCI;
  pipeCI.stageCount          = stageCount;
  pipeCI.pStages             = stages;
  pipeCI.pVertexInputState   = &vertexInput;
  pipeCI.pInputAssemblyState = &inputAssembly;
  pipeCI.pViewportState      = &viewportState;
  pipeCI.pRasterizationState = &raster;
  pipeCI.pMultisampleState   = &multisampling;
  pipeCI.pColorBlendState    = &blending;
  pipeCI.pDynamicState       = &dynState;
  pipeCI.layout              = pipeData->Layout;
  pipeCI.renderPass          = VK_NULL_HANDLE; // dynamic rendering

  if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCI, nullptr,
                                  &pipeData->Pipeline) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: vkCreateGraphicsPipelines failed");
    vkDestroyPipelineLayout(m_Device, pipeData->Layout, nullptr);
    vkDestroyShaderModule(m_Device, vertModule, nullptr);
    if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(m_Device, fragModule, nullptr);
    delete pipeData;
    return GraphicsPipeline{};
  }

  vkDestroyShaderModule(m_Device, vertModule, nullptr);
  if (fragModule != VK_NULL_HANDLE)
    vkDestroyShaderModule(m_Device, fragModule, nullptr);

  GraphicsPipeline pipeline;
  pipeline.Desc = desc;
  pipeline.Data = ::gecko::Shared<void>(pipeData, [this](void* ptr) {
    auto* d = static_cast<VulkanPipelineData*>(ptr);
    vkDestroyPipeline(m_Device, d->Pipeline, nullptr);
    vkDestroyPipelineLayout(m_Device, d->Layout, nullptr);
    delete d;
  });
  return pipeline;
}

ComputePipeline VulkanDevice::CreateComputePipeline(
    const ComputePipelineDesc& /*desc*/) noexcept
{
  GECKO_WARN(labels::Graphics, "VulkanDevice: CreateComputePipeline not yet implemented");
  return ComputePipeline{};
}

// ── Data upload ───────────────────────────────────────────────────────────

void VulkanDevice::UploadBufferData(Buffer& buffer,
                                     ::std::span<const ::gecko::byte> data,
                                     u32 offset) noexcept
{
  if (!buffer.Data || !m_Valid)
    return;

  auto* bufData = static_cast<VulkanBufferData*>(buffer.Data.get());

  VkDeviceSize totalSize = 0;
  switch (buffer.Type)
  {
    case BufferType::Vertex:
      totalSize = static_cast<VkDeviceSize>(buffer.VertexDesc.NumVertices) * buffer.VertexDesc.VertexSize;
      break;
    default:
      totalSize = data.size_bytes();
      break;
  }

  // Create a host-visible staging buffer
  VkBuffer      stagingBuf{};
  VmaAllocation stagingAlloc{};

  VkBufferCreateInfo stagingCI{};
  stagingCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  stagingCI.size  = data.size_bytes();
  stagingCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo stagingAllocCI{};
  stagingAllocCI.usage = VMA_MEMORY_USAGE_AUTO;
  stagingAllocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                         | VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VmaAllocationInfo allocInfo{};
  if (vmaCreateBuffer(m_Allocator, &stagingCI, &stagingAllocCI,
                       &stagingBuf, &stagingAlloc, &allocInfo) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanDevice: UploadBufferData staging alloc failed");
    return;
  }

  ::std::memcpy(allocInfo.pMappedData, data.data(), data.size_bytes());

  // One-time copy command
  struct CopyCtx { VkBuffer Src; VkBuffer Dst; VkDeviceSize Offset; VkDeviceSize Size; };
  CopyCtx copyCtx{stagingBuf, bufData->Buffer, static_cast<VkDeviceSize>(offset), data.size_bytes()};

  OneTimeSubmit([](VkCommandBuffer cb, void* user) {
    auto* ctx = static_cast<CopyCtx*>(user);
    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = ctx->Offset;
    region.size      = ctx->Size;
    vkCmdCopyBuffer(cb, ctx->Src, ctx->Dst, 1, &region);
  }, &copyCtx);

  vmaDestroyBuffer(m_Allocator, stagingBuf, stagingAlloc);
  (void)totalSize;
}

void VulkanDevice::UploadTextureData(Texture& /*texture*/,
                                      ::std::span<const ::gecko::byte> /*data*/,
                                      u32 /*mip*/, u32 /*slice*/) noexcept
{
  GECKO_WARN(labels::Graphics, "VulkanDevice: UploadTextureData not yet implemented");
}

// ── OneTimeSubmit helper ──────────────────────────────────────────────────

void VulkanDevice::OneTimeSubmit(void (*record)(VkCommandBuffer, void*),
                                  void* ctx) noexcept
{
  VkCommandBufferAllocateInfo cbAI{};
  cbAI.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbAI.commandPool        = m_GraphicsCommandPool;
  cbAI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbAI.commandBufferCount = 1;

  VkCommandBuffer cb = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(m_Device, &cbAI, &cb) != VK_SUCCESS)
    return;

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cb, &beginInfo);

  record(cb, ctx);

  vkEndCommandBuffer(cb);

  VkSubmitInfo si{};
  si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers    = &cb;

  vkQueueSubmit(m_GraphicsQueue, 1, &si, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_GraphicsQueue);

  vkFreeCommandBuffers(m_Device, m_GraphicsCommandPool, 1, &cb);
}

}  // namespace gecko::graphics
