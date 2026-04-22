#define VMA_IMPLEMENTATION 1
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

// VMA's heavy use of partial C-style initializers trips our -Werror set.
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#  pragma GCC diagnostic ignored "-Wunused-variable"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include "vulkan_device.h"

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

#include "vulkan_command_list.h"
#include "vulkan_surface.h"
#include "vulkan_util.h"

#include "gecko/core/services/log.h"
#include "gecko/core/services/memory.h"
#include "private/labels.h"

#include <cstring>
#include <new>
#include <string_view>
#include <vector>

namespace gecko::graphics {

// ── Small allocation helpers (route through Gecko allocator) ─────────────

template <typename T>
[[nodiscard]] static T* AllocObject() noexcept
{
  void* mem = ::gecko::AllocBytes(sizeof(T), alignof(T));
  if (mem == nullptr)
    return nullptr;
  return new (mem) T();
}

template <typename T>
static void FreeObject(T* obj) noexcept
{
  if (obj == nullptr)
    return;
  obj->~T();
  ::gecko::DeallocBytes(obj);
}

// ── Debug messenger callback ─────────────────────────────────────────────

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             /*types*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       /*userdata*/)
{
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    GECKO_ERROR(labels::Vulkan, "%s", data->pMessage);
  else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    GECKO_WARN(labels::Vulkan, "%s", data->pMessage);
  return VK_FALSE;
}

// ─────────────────────────────────────────────────────────────────────────
// VulkanDevice construction / teardown
// ─────────────────────────────────────────────────────────────────────────

VulkanDevice::VulkanDevice(const GraphicsDeviceDesc& desc) noexcept
{
  // ── Instance ──────────────────────────────────────────────────

  VkApplicationInfo app{};
  app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName   = desc.AppName;
  app.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
  app.pEngineName        = "Gecko";
  app.engineVersion      = VK_MAKE_VERSION(0, 0, 1);
  app.apiVersion         = VK_API_VERSION_1_3;

  auto surfaceExts = GetRequiredSurfaceExtensions();

  // ── Enumerate available layers / extensions (for graceful fallback) ──
  ::std::vector<VkExtensionProperties> availableExts;
  {
    u32 n = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
    availableExts.resize(n);
    vkEnumerateInstanceExtensionProperties(nullptr, &n, availableExts.data());
  }
  auto hasExt = [&](const char* name) {
    for (auto& e : availableExts)
      if (::std::string_view(e.extensionName) == name)
        return true;
    return false;
  };

  ::std::vector<const char*> instanceExts;
  for (auto* e : surfaceExts)
  {
    if (hasExt(e))
      instanceExts.push_back(e);
    else
      GECKO_WARN(labels::Vulkan,
                 "VulkanDevice: instance extension '%s' unavailable", e);
  }

  ::std::vector<VkLayerProperties> availableLayers;
  {
    u32 n = 0;
    vkEnumerateInstanceLayerProperties(&n, nullptr);
    availableLayers.resize(n);
    vkEnumerateInstanceLayerProperties(&n, availableLayers.data());
  }
  auto hasLayer = [&](const char* name) {
    for (auto& l : availableLayers)
      if (::std::string_view(l.layerName) == name)
        return true;
    return false;
  };

  bool haveDebugUtils = desc.Debug && hasExt(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  if (haveDebugUtils)
    instanceExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  else if (desc.Debug)
    GECKO_WARN(labels::Vulkan,
               "VulkanDevice: VK_EXT_debug_utils not available; "
               "debug messenger disabled");

  ::std::vector<const char*> layers;
  if (desc.Debug && hasLayer("VK_LAYER_KHRONOS_validation"))
    layers.push_back("VK_LAYER_KHRONOS_validation");
  else if (desc.Debug)
    GECKO_WARN(labels::Vulkan,
               "VulkanDevice: VK_LAYER_KHRONOS_validation not available; "
               "validation disabled");

  VkInstanceCreateInfo ici{};
  ici.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ici.pApplicationInfo        = &app;
  ici.enabledExtensionCount   = static_cast<u32>(instanceExts.size());
  ici.ppEnabledExtensionNames = instanceExts.data();
  ici.enabledLayerCount       = static_cast<u32>(layers.size());
  ici.ppEnabledLayerNames     = layers.data();

  if (vkCreateInstance(&ici, nullptr, &m_Instance) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice: vkCreateInstance failed "
                "(requested %u extensions, %u layers)",
                ici.enabledExtensionCount, ici.enabledLayerCount);
    for (u32 i = 0; i < ici.enabledExtensionCount; ++i)
      GECKO_ERROR(labels::Vulkan, "  ext: %s", instanceExts[i]);
    for (u32 i = 0; i < ici.enabledLayerCount; ++i)
      GECKO_ERROR(labels::Vulkan, "  layer: %s", layers[i]);
    return;
  }

  // Debug messenger
  if (haveDebugUtils)
  {
    VkDebugUtilsMessengerCreateInfoEXT dmi{};
    dmi.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dmi.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                          | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dmi.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                          | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                          | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dmi.pfnUserCallback = DebugCallback;
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
    if (fn != nullptr)
      fn(m_Instance, &dmi, nullptr, &m_DebugMessenger);
  }

  // ── Physical device ───────────────────────────────────────────

  u32 count = 0;
  vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
  if (count == 0)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: no Vulkan physical devices");
    return;
  }
  ::std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());

  // Prefer discrete GPU
  m_PhysicalDevice = devices[0];
  for (auto& pd : devices)
  {
    VkPhysicalDeviceProperties p{};
    vkGetPhysicalDeviceProperties(pd, &p);
    if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
      m_PhysicalDevice = pd;
      break;
    }
  }

  // Find graphics queue family
  u32 qcount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &qcount, nullptr);
  ::std::vector<VkQueueFamilyProperties> qfams(qcount);
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &qcount,
                                            qfams.data());
  m_GraphicsQueueFamily = UINT32_MAX;
  for (u32 i = 0; i < qcount; ++i)
  {
    if ((qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
    {
      m_GraphicsQueueFamily = i;
      break;
    }
  }
  if (m_GraphicsQueueFamily == UINT32_MAX)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: no graphics queue family");
    return;
  }
  m_PresentQueueFamily = m_GraphicsQueueFamily;  // assume unified

  // ── Logical device ────────────────────────────────────────────

  f32 priority = 1.0F;
  VkDeviceQueueCreateInfo qci{};
  qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex = m_GraphicsQueueFamily;
  qci.queueCount       = 1;
  qci.pQueuePriorities = &priority;

  ::std::vector<const char*> deviceExts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkPhysicalDeviceVulkan13Features f13{};
  f13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  f13.dynamicRendering = VK_TRUE;
  f13.synchronization2 = VK_TRUE;

  VkPhysicalDeviceFeatures2 f2{};
  f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  f2.pNext = &f13;

  VkDeviceCreateInfo dci{};
  dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.pNext                   = &f2;
  dci.queueCreateInfoCount    = 1;
  dci.pQueueCreateInfos       = &qci;
  dci.enabledExtensionCount   = static_cast<u32>(deviceExts.size());
  dci.ppEnabledExtensionNames = deviceExts.data();

  if (vkCreateDevice(m_PhysicalDevice, &dci, nullptr, &m_Device) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: vkCreateDevice failed");
    return;
  }

  vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
  m_PresentQueue = m_GraphicsQueue;

  // ── Command pool ──────────────────────────────────────────────

  VkCommandPoolCreateInfo cpci{};
  cpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cpci.queueFamilyIndex = m_GraphicsQueueFamily;
  VULKAN_CHECK(vkCreateCommandPool(m_Device, &cpci, nullptr,
                                    &m_GraphicsCommandPool));

  // ── VMA allocator ─────────────────────────────────────────────

  VmaVulkanFunctions vkfns{};
  vkfns.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vkfns.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo aci{};
  aci.instance         = m_Instance;
  aci.physicalDevice   = m_PhysicalDevice;
  aci.device           = m_Device;
  aci.vulkanApiVersion = VK_API_VERSION_1_3;
  aci.pVulkanFunctions = &vkfns;
  VULKAN_CHECK(vmaCreateAllocator(&aci, &m_Allocator));

  // ── Descriptor pool ───────────────────────────────────────────
  // Example-grade: a single large pool, never reset. Sufficient for the
  // current example (one or two BindTexture calls per frame, short-lived).
  // Production code should switch to per-frame ring pools.
  {
    VkDescriptorPoolSize sizes[1] {};
    sizes[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[0].descriptorCount = 4096;

    VkDescriptorPoolCreateInfo dpci {};
    dpci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets       = 4096;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes    = sizes;
    VULKAN_CHECK(
        vkCreateDescriptorPool(m_Device, &dpci, nullptr, &m_DescriptorPool));
  }

  m_Valid = true;
  GECKO_INFO(labels::Vulkan, "VulkanDevice ready");
}

VulkanDevice::~VulkanDevice()
{
  if (m_Device != VK_NULL_HANDLE)
    vkDeviceWaitIdle(m_Device);

  if (m_DescriptorPool != VK_NULL_HANDLE)
    vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

  if (m_Allocator != VK_NULL_HANDLE)
    vmaDestroyAllocator(m_Allocator);

  if (m_GraphicsCommandPool != VK_NULL_HANDLE)
    vkDestroyCommandPool(m_Device, m_GraphicsCommandPool, nullptr);

  if (m_Device != VK_NULL_HANDLE)
    vkDestroyDevice(m_Device, nullptr);

  if (m_DebugMessenger != VK_NULL_HANDLE)
  {
    auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (fn != nullptr)
      fn(m_Instance, m_DebugMessenger, nullptr);
  }

  if (m_Instance != VK_NULL_HANDLE)
    vkDestroyInstance(m_Instance, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────
// Swapchain
// ─────────────────────────────────────────────────────────────────────────

bool VulkanDevice::BuildSwapchainResources(VulkanSwapchainData& data,
                                            VkSwapchainKHR       oldSC) noexcept
{
  // Query surface caps
  VkSurfaceCapabilitiesKHR caps{};
  VULKAN_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice,
                                                          data.Surface, &caps));

  // Extent
  VkExtent2D extent{};
  if (caps.currentExtent.width != UINT32_MAX)
  {
    extent = caps.currentExtent;
  }
  else
  {
    extent.width  = data.Desc.Width;
    extent.height = data.Desc.Height;
  }
  if (extent.width == 0 || extent.height == 0)
  {
    GECKO_WARN(labels::Vulkan,
               "VulkanDevice: zero-size surface, deferring swapchain build");
    return false;
  }
  data.Extent = extent;

  // Format
  u32 fcount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, data.Surface, &fcount,
                                        nullptr);
  ::std::vector<VkSurfaceFormatKHR> formats(fcount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, data.Surface, &fcount,
                                        formats.data());

  VkFormat wanted = ToVkFormat(data.Desc.Format);
  VkSurfaceFormatKHR chosen = formats[0];
  for (auto& f : formats)
  {
    if (f.format == wanted
        && f.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
    {
      chosen = f;
      break;
    }
  }
  data.Format = chosen.format;

  // Present mode
  VkPresentModeKHR pm = data.Desc.VSync ? VK_PRESENT_MODE_FIFO_KHR
                                        : VK_PRESENT_MODE_IMMEDIATE_KHR;

  // Image count
  u32 imageCount = data.Desc.NumBackBuffers;
  if (imageCount < caps.minImageCount)
    imageCount = caps.minImageCount;
  if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
    imageCount = caps.maxImageCount;
  if (imageCount > MaxSwapchainImages)
    imageCount = MaxSwapchainImages;

  VkSwapchainCreateInfoKHR sci{};
  sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  sci.surface          = data.Surface;
  sci.minImageCount    = imageCount;
  sci.imageFormat      = chosen.format;
  sci.imageColorSpace  = chosen.colorSpace;
  sci.imageExtent      = extent;
  sci.imageArrayLayers = 1;
  sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                   | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  sci.preTransform     = caps.currentTransform;
  sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  sci.presentMode      = pm;
  sci.clipped          = VK_TRUE;
  sci.oldSwapchain     = oldSC;

  if (vkCreateSwapchainKHR(m_Device, &sci, nullptr, &data.Swapchain)
      != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: vkCreateSwapchainKHR failed");
    return false;
  }

  // Images + views
  u32 realCount = 0;
  vkGetSwapchainImagesKHR(m_Device, data.Swapchain, &realCount, nullptr);
  if (realCount > MaxSwapchainImages)
    realCount = MaxSwapchainImages;
  data.ImageCount = realCount;
  vkGetSwapchainImagesKHR(m_Device, data.Swapchain, &realCount, data.Images);

  for (u32 i = 0; i < realCount; ++i)
  {
    VkImageViewCreateInfo ivci{};
    ivci.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image      = data.Images[i];
    ivci.viewType   = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format     = chosen.format;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    VULKAN_CHECK(vkCreateImageView(m_Device, &ivci, nullptr,
                                    &data.ImageViews[i]));
  }

  // Sync objects (once, per-frame-in-flight)
  if (data.InFlight[0] == VK_NULL_HANDLE)
  {
    VkSemaphoreCreateInfo sem{};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fc{};
    fc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fc.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (u32 i = 0; i < MaxFramesInFlight; ++i)
    {
      VULKAN_CHECK(vkCreateSemaphore(m_Device, &sem, nullptr,
                                      &data.ImageAvailable[i]));
      VULKAN_CHECK(vkCreateSemaphore(m_Device, &sem, nullptr,
                                      &data.RenderFinished[i]));
      VULKAN_CHECK(vkCreateFence(m_Device, &fc, nullptr, &data.InFlight[i]));
    }
  }

  return true;
}

void VulkanDevice::DestroySwapchainResources(VulkanSwapchainData& data,
                                              bool destroySurface) noexcept
{
  for (u32 i = 0; i < data.ImageCount; ++i)
  {
    if (data.ImageViews[i] != VK_NULL_HANDLE)
    {
      vkDestroyImageView(m_Device, data.ImageViews[i], nullptr);
      data.ImageViews[i] = VK_NULL_HANDLE;
    }
  }
  data.ImageCount = 0;

  if (data.Swapchain != VK_NULL_HANDLE)
  {
    vkDestroySwapchainKHR(m_Device, data.Swapchain, nullptr);
    data.Swapchain = VK_NULL_HANDLE;
  }

  if (destroySurface)
  {
    for (u32 i = 0; i < MaxFramesInFlight; ++i)
    {
      if (data.ImageAvailable[i] != VK_NULL_HANDLE)
        vkDestroySemaphore(m_Device, data.ImageAvailable[i], nullptr);
      if (data.RenderFinished[i] != VK_NULL_HANDLE)
        vkDestroySemaphore(m_Device, data.RenderFinished[i], nullptr);
      if (data.InFlight[i] != VK_NULL_HANDLE)
        vkDestroyFence(m_Device, data.InFlight[i], nullptr);
      data.ImageAvailable[i] = VK_NULL_HANDLE;
      data.RenderFinished[i] = VK_NULL_HANDLE;
      data.InFlight[i]       = VK_NULL_HANDLE;
    }
    if (data.Surface != VK_NULL_HANDLE)
    {
      vkDestroySurfaceKHR(m_Instance, data.Surface, nullptr);
      data.Surface = VK_NULL_HANDLE;
    }
  }
}

Swapchain VulkanDevice::CreateSwapchain(
    const ::gecko::platform::NativeWindowHandle& native,
    const SwapchainDesc&                         desc) noexcept
{
  if (!m_Valid)
    return Swapchain{};

  VulkanSwapchainData* data = AllocObject<VulkanSwapchainData>();
  data->Native              = native;
  data->Desc                = desc;

  if (CreateSurface(m_Instance, native, &data->Surface) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: surface creation failed");
    FreeObject(data);
    return Swapchain{};
  }

  VkBool32 support = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, m_GraphicsQueueFamily,
                                        data->Surface, &support);
  if (support == VK_FALSE)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice: queue family cannot present to surface");
    DestroySwapchainResources(*data, true);
    FreeObject(data);
    return Swapchain{};
  }

  if (!BuildSwapchainResources(*data, VK_NULL_HANDLE))
  {
    DestroySwapchainResources(*data, true);
    FreeObject(data);
    return Swapchain{};
  }

  Swapchain sc;
  sc.Desc = desc;
  sc.Desc.Width  = data->Extent.width;
  sc.Desc.Height = data->Extent.height;
  sc.Desc.Format = FromVkFormat(data->Format);
  // Shared<void> with custom deleter keeping a device ref
  VulkanDevice* dev = this;
  sc.Data = Shared<void>(data, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanSwapchainData*>(p);
    vkDeviceWaitIdle(dev->m_Device);
    dev->DestroySwapchainResources(*d, true);
    FreeObject(d);
  });
  return sc;
}

void VulkanDevice::DestroySwapchain(Swapchain& swapchain) noexcept
{
  swapchain = Swapchain{};
}

void VulkanDevice::ResizeSwapchain(Swapchain& swapchain) noexcept
{
  if (!swapchain.Data)
    return;
  auto* data = static_cast<VulkanSwapchainData*>(swapchain.Data.get());
  // Sync user-visible desc back into internal data so the Wayland fallback
  // path (where VkSurfaceCapabilities.currentExtent is always UINT32_MAX)
  // sees the new requested size.  On X11/Win32 currentExtent has real
  // values and this is a no-op.
  data->Desc.Width  = swapchain.Desc.Width;
  data->Desc.Height = swapchain.Desc.Height;
  const u32 oldW = data->Extent.width;
  const u32 oldH = data->Extent.height;
  vkDeviceWaitIdle(m_Device);
  VkSwapchainKHR old = data->Swapchain;
  data->Swapchain    = VK_NULL_HANDLE;
  // Destroy image views but keep surface + sync
  for (u32 i = 0; i < data->ImageCount; ++i)
  {
    if (data->ImageViews[i] != VK_NULL_HANDLE)
    {
      vkDestroyImageView(m_Device, data->ImageViews[i], nullptr);
      data->ImageViews[i] = VK_NULL_HANDLE;
    }
  }
  (void)BuildSwapchainResources(*data, old);
  if (old != VK_NULL_HANDLE)
    vkDestroySwapchainKHR(m_Device, old, nullptr);

  swapchain.Desc.Width  = data->Extent.width;
  swapchain.Desc.Height = data->Extent.height;
  GECKO_INFO(labels::Vulkan,
             "VulkanDevice: swapchain resized %ux%u -> %ux%u",
             oldW, oldH, data->Extent.width, data->Extent.height);
}

FrameContext VulkanDevice::BeginFrame(Swapchain& swapchain) noexcept
{
  FrameContext ctx{};
  if (!swapchain.Data)
    return ctx;
  auto* data = static_cast<VulkanSwapchainData*>(swapchain.Data.get());
  if (data->Swapchain == VK_NULL_HANDLE)
    return ctx;

  const u32 frame = data->FrameIndex;
  vkWaitForFences(m_Device, 1, &data->InFlight[frame], VK_TRUE, UINT64_MAX);

  u32       imageIndex = 0;
  VkResult  ar = vkAcquireNextImageKHR(m_Device, data->Swapchain, UINT64_MAX,
                                        data->ImageAvailable[frame],
                                        VK_NULL_HANDLE, &imageIndex);
  if (ar == VK_ERROR_OUT_OF_DATE_KHR)
  {
    ResizeSwapchain(swapchain);
    return ctx;  // skip this frame
  }
  if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: acquire failed (%d)",
                static_cast<i32>(ar));
    return ctx;
  }

  vkResetFences(m_Device, 1, &data->InFlight[frame]);

  data->AcquiredIndex = imageIndex;

  // Build a RenderTarget wrapper around the acquired image.
  VulkanRTData* rtd = AllocObject<VulkanRTData>();
  rtd->RTKind        = VulkanRTData::Kind::Swapchain;
  rtd->Image         = data->Images[imageIndex];
  rtd->ImageView     = data->ImageViews[imageIndex];
  rtd->SwapchainData = data;
  rtd->FrameIndex    = frame;

  RenderTarget rt;
  rt.Desc.Width                  = data->Extent.width;
  rt.Desc.Height                 = data->Extent.height;
  rt.Desc.NumRenderTargets       = 1;
  rt.Desc.RenderTargetFormats[0] = FromVkFormat(data->Format);
  rt.Data = Shared<void>(rtd, [](void* p) noexcept {
    FreeObject(static_cast<VulkanRTData*>(p));
  });

  ctx.SC         = &swapchain;
  ctx.FrameIndex = frame;
  ctx.ImageIndex = imageIndex;
  ctx.BackBuffer = ::std::move(rt);
  ctx.Valid      = true;
  return ctx;
}

void VulkanDevice::Present(::std::span<const FrameContext> frames) noexcept
{
  if (frames.empty())
    return;

  VkSwapchainKHR scs[MaxSwapchainsPerSubmit]{};
  u32            indices[MaxSwapchainsPerSubmit]{};
  VkSemaphore    waits[MaxSwapchainsPerSubmit]{};
  u32            count = 0;

  for (const auto& f : frames)
  {
    if (!f.Valid || f.SC == nullptr || !f.SC->Data)
      continue;
    if (count >= MaxSwapchainsPerSubmit)
    {
      GECKO_WARN(labels::Vulkan,
                 "VulkanDevice::Present: more than %u swapchains, truncated",
                 MaxSwapchainsPerSubmit);
      break;
    }
    auto* data = static_cast<VulkanSwapchainData*>(f.SC->Data.get());
    scs[count]     = data->Swapchain;
    indices[count] = f.ImageIndex;
    waits[count]   = data->RenderFinished[f.FrameIndex];
    ++count;
  }

  if (count == 0)
    return;

  VkPresentInfoKHR pi{};
  pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  pi.waitSemaphoreCount = count;
  pi.pWaitSemaphores    = waits;
  pi.swapchainCount     = count;
  pi.pSwapchains        = scs;
  pi.pImageIndices      = indices;

  VkResult pr = vkQueuePresentKHR(m_PresentQueue, &pi);
  if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
  {
    for (const auto& f : frames)
      if (f.Valid && f.SC != nullptr)
        ResizeSwapchain(*const_cast<Swapchain*>(f.SC));
  }

  // Advance frame indices
  for (const auto& f : frames)
  {
    if (!f.Valid || f.SC == nullptr || !f.SC->Data)
      continue;
    auto* data       = static_cast<VulkanSwapchainData*>(f.SC->Data.get());
    data->FrameIndex = (data->FrameIndex + 1) % MaxFramesInFlight;
  }
}

// ─────────────────────────────────────────────────────────────────────────
// Command lists
// ─────────────────────────────────────────────────────────────────────────

Unique<ICommandList> VulkanDevice::CreateGraphicsCommandList() noexcept
{
  return CreateUnique<VulkanCommandList>(*this, /*compute*/ false);
}

Unique<ICommandList> VulkanDevice::CreateComputeCommandList() noexcept
{
  return CreateUnique<VulkanCommandList>(*this, /*compute*/ true);
}

void VulkanDevice::ExecuteGraphicsCommandList(
    Unique<ICommandList> commandList) noexcept
{
  auto* cl = static_cast<VulkanCommandList*>(commandList.get());
  if (cl == nullptr || !cl->IsValid())
    return;

  VkCommandBuffer cb = cl->CommandBuffer();

  // Gather wait/signal semaphores from touched swapchains
  constexpr u32 kMax = MaxSwapchainsPerSubmit;
  VkSemaphore waitSems[kMax]{};
  VkSemaphore sigSems[kMax]{};
  VkPipelineStageFlags waitStages[kMax]{};
  u32 waitCount = 0;
  u32 sigCount  = 0;

  auto touched = cl->TouchedSwapchains();
  for (u32 i = 0; i < touched.Count && i < kMax; ++i)
  {
    auto* data = touched.Data[i].Data;
    u32   idx  = touched.Data[i].FrameIndex;
    waitSems[waitCount]    = data->ImageAvailable[idx];
    waitStages[waitCount]  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    ++waitCount;
    sigSems[sigCount]      = data->RenderFinished[idx];
    ++sigCount;
  }

  VkSubmitInfo si{};
  si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount   = 1;
  si.pCommandBuffers      = &cb;
  si.waitSemaphoreCount   = waitCount;
  si.pWaitSemaphores      = waitSems;
  si.pWaitDstStageMask    = waitStages;
  si.signalSemaphoreCount = sigCount;
  si.pSignalSemaphores    = sigSems;

  // Signal the first touched swapchain's InFlight fence with the real
  // submit. Every other touched swapchain needs its own fence signaled too,
  // otherwise BeginFrame will block forever after `MaxFramesInFlight` cycles
  // (its per-swapchain vkWaitForFences would never complete). Use cheap
  // empty signaling submits for the remaining fences.
  VkFence primaryFence = VK_NULL_HANDLE;
  if (touched.Count >= 1)
  {
    auto* d0     = touched.Data[0].Data;
    primaryFence = d0->InFlight[touched.Data[0].FrameIndex];
  }

  VULKAN_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &si, primaryFence));

  for (u32 i = 1; i < touched.Count; ++i)
  {
    auto*   d = touched.Data[i].Data;
    VkFence f = d->InFlight[touched.Data[i].FrameIndex];
    VkSubmitInfo empty{};
    empty.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VULKAN_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &empty, f));
  }

  // commandList is freed here (Unique dies at scope end)
  (void)commandList;
}

void VulkanDevice::ExecuteComputeCommandList(
    Unique<ICommandList> commandList) noexcept
{
  ExecuteGraphicsCommandList(::std::move(commandList));
}

// ─────────────────────────────────────────────────────────────────────────
// Resource creation (stubs for now — triangle path doesn't need most)
// ─────────────────────────────────────────────────────────────────────────

RenderTarget VulkanDevice::CreateRenderTarget(
    const RenderTargetDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return RenderTarget{};

  RenderTarget rt;
  rt.Desc = desc;

  auto* rtd    = AllocObject<VulkanRTData>();
  rtd->RTKind  = VulkanRTData::Kind::Offscreen;

  // ── Colour textures ───────────────────────────────────────────
  for (u32 i = 0; i < desc.NumRenderTargets; ++i)
  {
    TextureDesc td{};
    td.Width          = desc.Width;
    td.Height         = desc.Height;
    td.Depth          = 1;
    td.NumMips        = 1;
    td.NumArraySlices = 1;
    td.Format         = desc.RenderTargetFormats[i];
    td.Type           = TextureType::Tex2D;
    td.Memory         = MemoryType::Dedicated;
    td.IsRenderTarget = true;
    td.OptimizedClear = desc.RenderTargetClearValues[i];

    Texture t = CreateTexture(td);
    if (!t.IsValid())
    {
      GECKO_ERROR(labels::Vulkan,
                  "VulkanDevice::CreateRenderTarget: colour texture %u failed",
                  i);
      FreeObject(rtd);
      return RenderTarget{};
    }

    auto* texData         = static_cast<VulkanTextureData*>(t.Data.get());
    rtd->OffscreenTex[i]  = texData;
    if (i == 0)
    {
      rtd->Image     = texData->Image;
      rtd->ImageView = texData->ImageView;
    }
    rt.RenderTextures[i] = ::std::move(t);
  }
  rtd->NumOffscreen = desc.NumRenderTargets;

  // ── Depth texture (optional) ──────────────────────────────────
  if (desc.DepthStencilFormat != DataFormat::None)
  {
    TextureDesc td{};
    td.Width          = desc.Width;
    td.Height         = desc.Height;
    td.Depth          = 1;
    td.NumMips        = 1;
    td.NumArraySlices = 1;
    td.Format         = desc.DepthStencilFormat;
    td.Type           = TextureType::Tex2D;
    td.Memory         = MemoryType::Dedicated;
    td.IsDepthStencil = true;
    td.OptimizedClear = desc.DepthStencilClearValue;

    Texture d = CreateTexture(td);
    if (d.IsValid())
    {
      rtd->OffscreenDepth = static_cast<VulkanTextureData*>(d.Data.get());
      rt.DepthTexture     = ::std::move(d);
    }
  }

  rt.Data = Shared<void>(rtd, [](void* p) noexcept {
    FreeObject(static_cast<VulkanRTData*>(p));
  });
  return rt;
}

Buffer VulkanDevice::CreateVertexBuffer(const VertexBufferDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return Buffer{};

  const u32 size = desc.NumVertices * desc.VertexSize;

  VkBufferCreateInfo bci{};
  bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bci.size  = size;
  bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo aci{};
  aci.usage = (desc.Memory == MemoryType::Shared)
                  ? VMA_MEMORY_USAGE_CPU_TO_GPU
                  : VMA_MEMORY_USAGE_GPU_ONLY;

  VulkanBufferData* bd = AllocObject<VulkanBufferData>();
  if (vmaCreateBuffer(m_Allocator, &bci, &aci, &bd->Buffer, &bd->Allocation,
                       nullptr)
      != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice::CreateVertexBuffer vmaCreateBuffer failed");
    FreeObject(bd);
    return Buffer{};
  }

  Buffer b;
  b.Type       = BufferType::Vertex;
  b.VertexDesc = desc;
  VulkanDevice* dev = this;
  b.Data = Shared<void>(bd, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanBufferData*>(p);
    if (d->Buffer != VK_NULL_HANDLE)
      vmaDestroyBuffer(dev->m_Allocator, d->Buffer, d->Allocation);
    FreeObject(d);
  });
  return b;
}

Buffer VulkanDevice::CreateIndexBuffer(const IndexBufferDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return Buffer{};

  const u32 size = desc.NumIndices * sizeof(u32);

  VkBufferCreateInfo bci{};
  bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bci.size  = size;
  bci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo aci{};
  aci.usage = (desc.Memory == MemoryType::Shared)
                  ? VMA_MEMORY_USAGE_CPU_TO_GPU
                  : VMA_MEMORY_USAGE_GPU_ONLY;

  VulkanBufferData* bd = AllocObject<VulkanBufferData>();
  if (vmaCreateBuffer(m_Allocator, &bci, &aci, &bd->Buffer, &bd->Allocation,
                       nullptr)
      != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice::CreateIndexBuffer vmaCreateBuffer failed");
    FreeObject(bd);
    return Buffer{};
  }

  Buffer b;
  b.Type      = BufferType::Index;
  b.IndexDesc = desc;
  VulkanDevice* dev = this;
  b.Data = Shared<void>(bd, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanBufferData*>(p);
    if (d->Buffer != VK_NULL_HANDLE)
      vmaDestroyBuffer(dev->m_Allocator, d->Buffer, d->Allocation);
    FreeObject(d);
  });
  return b;
}

Buffer VulkanDevice::CreateConstantBuffer(const ConstantBufferDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return Buffer{};

  VkBufferCreateInfo bci{};
  bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bci.size        = desc.SizeInBytes;
  bci.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo aci{};
  aci.usage = (desc.Memory == MemoryType::Shared)
                  ? VMA_MEMORY_USAGE_CPU_TO_GPU
                  : VMA_MEMORY_USAGE_GPU_ONLY;
  if (desc.Memory == MemoryType::Shared)
    aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
              | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VulkanBufferData* bd = AllocObject<VulkanBufferData>();
  if (vmaCreateBuffer(m_Allocator, &bci, &aci, &bd->Buffer, &bd->Allocation,
                       nullptr)
      != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice::CreateConstantBuffer vmaCreateBuffer failed");
    FreeObject(bd);
    return Buffer{};
  }

  Buffer b;
  b.Type         = BufferType::Constant;
  b.ConstantDesc = desc;
  VulkanDevice* dev = this;
  b.Data = Shared<void>(bd, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanBufferData*>(p);
    if (d->Buffer != VK_NULL_HANDLE)
      vmaDestroyBuffer(dev->m_Allocator, d->Buffer, d->Allocation);
    FreeObject(d);
  });
  return b;
}

Buffer VulkanDevice::CreateStructuredBuffer(
    const StructuredBufferDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return Buffer{};

  const VkDeviceSize size = static_cast<VkDeviceSize>(desc.NumElements)
                           * desc.ElementSize;

  VkBufferCreateInfo bci{};
  bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bci.size        = size;
  bci.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT
               | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo aci{};
  aci.usage = (desc.Memory == MemoryType::Shared)
                  ? VMA_MEMORY_USAGE_CPU_TO_GPU
                  : VMA_MEMORY_USAGE_GPU_ONLY;
  if (desc.Memory == MemoryType::Shared)
    aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
              | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VulkanBufferData* bd = AllocObject<VulkanBufferData>();
  if (vmaCreateBuffer(m_Allocator, &bci, &aci, &bd->Buffer, &bd->Allocation,
                       nullptr)
      != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice::CreateStructuredBuffer vmaCreateBuffer failed");
    FreeObject(bd);
    return Buffer{};
  }

  Buffer b;
  b.Type           = BufferType::Structured;
  b.StructuredDesc = desc;
  VulkanDevice* dev = this;
  b.Data = Shared<void>(bd, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanBufferData*>(p);
    if (d->Buffer != VK_NULL_HANDLE)
      vmaDestroyBuffer(dev->m_Allocator, d->Buffer, d->Allocation);
    FreeObject(d);
  });
  return b;
}

Texture VulkanDevice::CreateTexture(const TextureDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return Texture{};

  const VkFormat fmt  = ToVkFormat(desc.Format);
  const bool isDepth  = IsDepthFormat(desc.Format);

  VkImageCreateInfo ici{};
  ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ici.imageType     = VK_IMAGE_TYPE_2D;
  ici.format        = fmt;
  ici.extent        = {desc.Width, desc.Height, 1};
  ici.mipLevels     = desc.NumMips > 0 ? desc.NumMips : 1;
  ici.arrayLayers   = desc.NumArraySlices > 0 ? desc.NumArraySlices : 1;
  ici.samples       = VK_SAMPLE_COUNT_1_BIT;
  ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
  ici.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (desc.IsRenderTarget)
    ici.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (desc.IsDepthStencil || isDepth)
    ici.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
  ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo aci{};
  aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  auto* td            = AllocObject<VulkanTextureData>();
  td->Format          = fmt;
  td->Width           = desc.Width;
  td->Height          = desc.Height;
  td->IsRenderTarget  = desc.IsRenderTarget || desc.IsDepthStencil;
  td->Aspect          = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT
                                : VK_IMAGE_ASPECT_COLOR_BIT;

  if (vmaCreateImage(m_Allocator, &ici, &aci, &td->Image, &td->Allocation,
                      nullptr)
      != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice::CreateTexture vmaCreateImage failed");
    FreeObject(td);
    return Texture{};
  }

  VkImageViewCreateInfo ivci{};
  ivci.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  ivci.image        = td->Image;
  ivci.viewType     = VK_IMAGE_VIEW_TYPE_2D;
  ivci.format       = fmt;
  ivci.subresourceRange.aspectMask = td->Aspect;
  ivci.subresourceRange.levelCount = ici.mipLevels;
  ivci.subresourceRange.layerCount = ici.arrayLayers;
  if (vkCreateImageView(m_Device, &ivci, nullptr, &td->ImageView) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice::CreateTexture vkCreateImageView failed");
    vmaDestroyImage(m_Allocator, td->Image, td->Allocation);
    FreeObject(td);
    return Texture{};
  }

  Texture t;
  t.Desc = desc;
  VulkanDevice* dev = this;
  t.Data = Shared<void>(td, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanTextureData*>(p);
    if (d->ImageView != VK_NULL_HANDLE)
      vkDestroyImageView(dev->m_Device, d->ImageView, nullptr);
    if (d->Image != VK_NULL_HANDLE)
      vmaDestroyImage(dev->m_Allocator, d->Image, d->Allocation);
    FreeObject(d);
  });
  return t;
}

// ─────────────────────────────────────────────────────────────────────────
// Pipelines
// ─────────────────────────────────────────────────────────────────────────

VkShaderModule VulkanDevice::CreateShaderModule(const ShaderCode& code) noexcept
{
  if (!code.IsValid())
    return VK_NULL_HANDLE;
  if (code.Format != ShaderFormat::SPIRV)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice: only SPIRV shaders accepted (format=%d)",
                static_cast<i32>(code.Format));
    return VK_NULL_HANDLE;
  }

  VkShaderModuleCreateInfo smci{};
  smci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  smci.codeSize = code.Bytes.size();
  smci.pCode    = reinterpret_cast<const u32*>(code.Bytes.data());

  VkShaderModule m = VK_NULL_HANDLE;
  if (vkCreateShaderModule(m_Device, &smci, nullptr, &m) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: CreateShaderModule failed");
    return VK_NULL_HANDLE;
  }
  return m;
}

GraphicsPipeline VulkanDevice::CreateGraphicsPipeline(
    const GraphicsPipelineDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return GraphicsPipeline{};

  VkShaderModule vs = CreateShaderModule(desc.VertexShader);
  VkShaderModule ps = CreateShaderModule(desc.PixelShader);
  if (vs == VK_NULL_HANDLE)
    return GraphicsPipeline{};

  VkPipelineShaderStageCreateInfo stages[2]{};
  u32 stageCount = 0;

  stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[stageCount].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[stageCount].module = vs;
  stages[stageCount].pName  = desc.VertexShader.Entry;
  ++stageCount;

  if (ps != VK_NULL_HANDLE)
  {
    stages[stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[stageCount].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[stageCount].module = ps;
    stages[stageCount].pName  = desc.PixelShader.Entry;
    ++stageCount;
  }

  // Vertex input
  VkVertexInputBindingDescription vib{};
  vib.binding   = 0;
  vib.stride    = desc.Layout.StrideInBytes;
  vib.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription viattrs[VertexLayout::MaxAttributes]{};
  for (u32 i = 0; i < desc.Layout.NumAttributes; ++i)
  {
    viattrs[i].location = i;
    viattrs[i].binding  = 0;
    viattrs[i].format   = ToVkFormat(desc.Layout.Attributes[i].AttributeFormat);
    viattrs[i].offset   = desc.Layout.Attributes[i].Offset;
  }

  VkPipelineVertexInputStateCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  if (desc.Layout.NumAttributes > 0)
  {
    vi.vertexBindingDescriptionCount   = 1;
    vi.pVertexBindingDescriptions      = &vib;
    vi.vertexAttributeDescriptionCount = desc.Layout.NumAttributes;
    vi.pVertexAttributeDescriptions    = viattrs;
  }

  VkPipelineInputAssemblyStateCreateInfo ia{};
  ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia.topology = (desc.Primitive == PrimitiveType::Lines)
                    ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                    : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo vp{};
  vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vp.viewportCount = 1;
  vp.scissorCount  = 1;

  VkPipelineRasterizationStateCreateInfo rs{};
  rs.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode    = (desc.Culling == CullMode::Back)  ? VK_CULL_MODE_BACK_BIT
                   : (desc.Culling == CullMode::Front) ? VK_CULL_MODE_FRONT_BIT
                                                       : VK_CULL_MODE_NONE;
  rs.frontFace   = (desc.Winding == WindingOrder::ClockWise)
                       ? VK_FRONT_FACE_CLOCKWISE
                       : VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.lineWidth = 1.0F;

  VkPipelineMultisampleStateCreateInfo ms{};
  ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState cba[RenderTargetDesc::MaxRenderTargets]{};
  for (u32 i = 0; i < desc.NumRenderTargets; ++i)
  {
    cba[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                            | VK_COLOR_COMPONENT_B_BIT
                            | VK_COLOR_COMPONENT_A_BIT;
  }

  VkPipelineColorBlendStateCreateInfo cb{};
  cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.attachmentCount = desc.NumRenderTargets;
  cb.pAttachments    = cba;

  VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dyns{};
  dyns.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dyns.dynamicStateCount = sizeof(dyn) / sizeof(dyn[0]);
  dyns.pDynamicStates    = dyn;

  VkPipelineLayoutCreateInfo plci{};
  plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

  // ── Descriptor set layout & samplers ──────────────────────────
  // Resources are laid out as consecutive bindings in the order they
  // appear in `PipelineResources`, each `PipelineResource` contributing
  // `NumResources` individual descriptor slots of its type.
  VkSampler             samplers[GraphicsPipelineDesc::MaxSamplers] {};
  u32                   numSamplers = 0;
  VkDescriptorSetLayout dsl         = VK_NULL_HANDLE;
  VkDescriptorType      bindingTypes[VulkanPipelineData::MaxBindings] {};
  u32                   numBindings = 0;
  u32                   textureBindings = 0;  // subset of numBindings

  for (u32 i = 0; i < desc.NumSamplers; ++i)
  {
    const auto& sd = desc.SamplerDescs[i];
    VkSamplerCreateInfo sci{};
    sci.sType     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = sd.Filter == SamplerFilter::Linear ? VK_FILTER_LINEAR
                                                        : VK_FILTER_NEAREST;
    sci.minFilter  = sci.magFilter;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    const VkSamplerAddressMode am = sd.WrapMode == SamplerWrapMode::Wrap
                                         ? VK_SAMPLER_ADDRESS_MODE_REPEAT
                                         : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeU = am;
    sci.addressModeV = am;
    sci.addressModeW = am;
    sci.maxLod       = VK_LOD_CLAMP_NONE;
    VULKAN_CHECK(vkCreateSampler(m_Device, &sci, nullptr, &samplers[numSamplers]));
    ++numSamplers;
  }

  VkDescriptorSetLayoutBinding bindings[VulkanPipelineData::MaxBindings] {};
  VkSampler immSamplers[VulkanPipelineData::MaxBindings] {};

  for (u32 r = 0; r < desc.NumPipelineResources; ++r)
  {
    const auto& pr = desc.PipelineResources[r];
    for (u32 n = 0; n < pr.NumResources
                  && numBindings < VulkanPipelineData::MaxBindings;
         ++n)
    {
      VkDescriptorType dtype = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      switch (pr.Type)
      {
        case ResourceType::Texture:
          dtype = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
          ++textureBindings;
          break;
        case ResourceType::ConstantBuffer:
          dtype = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
          break;
        case ResourceType::StructuredBuffer:
          dtype = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          break;
        default: continue;
      }
      const u32 slot = numBindings;
      bindings[slot].binding         = slot;
      bindings[slot].descriptorType  = dtype;
      bindings[slot].descriptorCount = 1;
      bindings[slot].stageFlags      = VK_SHADER_STAGE_ALL_GRAPHICS;
      if (dtype == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
          && numSamplers > 0)
      {
        immSamplers[slot]              = samplers[0];
        bindings[slot].pImmutableSamplers = &immSamplers[slot];
      }
      bindingTypes[slot] = dtype;
      ++numBindings;
    }
  }

  if (numBindings > 0)
  {
    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = numBindings;
    dslci.pBindings    = bindings;
    VULKAN_CHECK(
        vkCreateDescriptorSetLayout(m_Device, &dslci, nullptr, &dsl));
    plci.setLayoutCount = 1;
    plci.pSetLayouts    = &dsl;
  }

  VkPipelineLayout layout = VK_NULL_HANDLE;
  VULKAN_CHECK(vkCreatePipelineLayout(m_Device, &plci, nullptr, &layout));

  // Dynamic rendering info (VK_KHR_dynamic_rendering core in 1.3)
  VkFormat colorFormats[RenderTargetDesc::MaxRenderTargets]{};
  for (u32 i = 0; i < desc.NumRenderTargets; ++i)
    colorFormats[i] = ToVkFormat(desc.RenderTargetFormats[i]);

  VkPipelineRenderingCreateInfo rci{};
  rci.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rci.colorAttachmentCount    = desc.NumRenderTargets;
  rci.pColorAttachmentFormats = colorFormats;
  rci.depthAttachmentFormat   = ToVkFormat(desc.DepthStencilFormat);

  VkGraphicsPipelineCreateInfo gpci{};
  gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  gpci.pNext               = &rci;
  gpci.stageCount          = stageCount;
  gpci.pStages             = stages;
  gpci.pVertexInputState   = &vi;
  gpci.pInputAssemblyState = &ia;
  gpci.pViewportState      = &vp;
  gpci.pRasterizationState = &rs;
  gpci.pMultisampleState   = &ms;
  gpci.pColorBlendState    = &cb;
  gpci.pDynamicState       = &dyns;
  gpci.layout              = layout;

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkResult res = vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &gpci,
                                            nullptr, &pipeline);
  vkDestroyShaderModule(m_Device, vs, nullptr);
  if (ps != VK_NULL_HANDLE)
    vkDestroyShaderModule(m_Device, ps, nullptr);

  if (res != VK_SUCCESS)
  {
    vkDestroyPipelineLayout(m_Device, layout, nullptr);
    if (dsl != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(m_Device, dsl, nullptr);
    for (u32 i = 0; i < numSamplers; ++i)
      vkDestroySampler(m_Device, samplers[i], nullptr);
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice: vkCreateGraphicsPipelines failed (%d)",
                static_cast<i32>(res));
    return GraphicsPipeline{};
  }

  VulkanPipelineData* pd = AllocObject<VulkanPipelineData>();
  pd->Layout             = layout;
  pd->Pipeline           = pipeline;
  pd->DescSetLayout      = dsl;
  pd->NumSamplers        = numSamplers;
  pd->NumTextureBindings = textureBindings;
  pd->NumBindings        = numBindings;
  pd->IsCompute          = false;
  for (u32 i = 0; i < numSamplers; ++i)
    pd->Samplers[i] = samplers[i];
  for (u32 i = 0; i < numBindings; ++i)
    pd->BindingTypes[i] = bindingTypes[i];

  GraphicsPipeline gp;
  gp.Desc = desc;
  VulkanDevice* dev = this;
  gp.Data = Shared<void>(pd, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanPipelineData*>(p);
    if (d->Pipeline != VK_NULL_HANDLE)
      vkDestroyPipeline(dev->m_Device, d->Pipeline, nullptr);
    if (d->Layout != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(dev->m_Device, d->Layout, nullptr);
    if (d->DescSetLayout != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(dev->m_Device, d->DescSetLayout, nullptr);
    for (u32 i = 0; i < d->NumSamplers; ++i)
      if (d->Samplers[i] != VK_NULL_HANDLE)
        vkDestroySampler(dev->m_Device, d->Samplers[i], nullptr);
    FreeObject(d);
  });
  return gp;
}

ComputePipeline VulkanDevice::CreateComputePipeline(
    const ComputePipelineDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return ComputePipeline{};

  VkShaderModule cs = CreateShaderModule(desc.ComputeShader);
  if (cs == VK_NULL_HANDLE)
    return ComputePipeline{};

  // Samplers
  VkSampler samplers[ComputePipelineDesc::MaxSamplers] {};
  u32       numSamplers = 0;
  for (u32 i = 0; i < desc.NumSamplers; ++i)
  {
    const auto& sd = desc.SamplerDescs[i];
    VkSamplerCreateInfo sci{};
    sci.sType     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = sd.Filter == SamplerFilter::Linear ? VK_FILTER_LINEAR
                                                        : VK_FILTER_NEAREST;
    sci.minFilter  = sci.magFilter;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    const VkSamplerAddressMode am = sd.WrapMode == SamplerWrapMode::Wrap
                                         ? VK_SAMPLER_ADDRESS_MODE_REPEAT
                                         : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeU = am;
    sci.addressModeV = am;
    sci.addressModeW = am;
    sci.maxLod       = VK_LOD_CLAMP_NONE;
    VULKAN_CHECK(
        vkCreateSampler(m_Device, &sci, nullptr, &samplers[numSamplers]));
    ++numSamplers;
  }

  // Bindings: read-only first, then read-write. Textures -> combined
  // image sampler (sampled) / storage image (RW); buffers -> uniform /
  // storage.
  VkDescriptorSetLayoutBinding bindings[VulkanPipelineData::MaxBindings] {};
  VkSampler immSamplers[VulkanPipelineData::MaxBindings] {};
  VkDescriptorType bindingTypes[VulkanPipelineData::MaxBindings] {};
  u32 numBindings = 0;

  auto addGroup = [&](const PipelineResource* arr, u32 count, bool readOnly) {
    for (u32 r = 0; r < count; ++r)
    {
      const auto& pr = arr[r];
      for (u32 n = 0; n < pr.NumResources
                    && numBindings < VulkanPipelineData::MaxBindings;
           ++n)
      {
        VkDescriptorType dtype = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        switch (pr.Type)
        {
          case ResourceType::Texture:
            dtype = readOnly ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                              : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            break;
          case ResourceType::ConstantBuffer:
            dtype = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
          case ResourceType::StructuredBuffer:
            dtype = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
          default: continue;
        }
        const u32 slot = numBindings;
        bindings[slot].binding         = slot;
        bindings[slot].descriptorType  = dtype;
        bindings[slot].descriptorCount = 1;
        bindings[slot].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        if (dtype == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
            && numSamplers > 0)
        {
          immSamplers[slot]                 = samplers[0];
          bindings[slot].pImmutableSamplers = &immSamplers[slot];
        }
        bindingTypes[slot] = dtype;
        ++numBindings;
      }
    }
  };

  addGroup(desc.ReadOnlyResources,  desc.NumReadOnlyResources,  true);
  addGroup(desc.ReadWriteResources, desc.NumReadWriteResources, false);

  VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
  VkPipelineLayoutCreateInfo plci{};
  plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  if (numBindings > 0)
  {
    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = numBindings;
    dslci.pBindings    = bindings;
    VULKAN_CHECK(
        vkCreateDescriptorSetLayout(m_Device, &dslci, nullptr, &dsl));
    plci.setLayoutCount = 1;
    plci.pSetLayouts    = &dsl;
  }

  VkPipelineLayout layout = VK_NULL_HANDLE;
  VULKAN_CHECK(vkCreatePipelineLayout(m_Device, &plci, nullptr, &layout));

  VkPipelineShaderStageCreateInfo stage{};
  stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
  stage.module = cs;
  stage.pName  = desc.ComputeShader.Entry;

  VkComputePipelineCreateInfo cpci{};
  cpci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  cpci.stage  = stage;
  cpci.layout = layout;

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkResult   res = vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &cpci,
                                             nullptr, &pipeline);
  vkDestroyShaderModule(m_Device, cs, nullptr);

  if (res != VK_SUCCESS)
  {
    vkDestroyPipelineLayout(m_Device, layout, nullptr);
    if (dsl != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(m_Device, dsl, nullptr);
    for (u32 i = 0; i < numSamplers; ++i)
      vkDestroySampler(m_Device, samplers[i], nullptr);
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice: vkCreateComputePipelines failed (%d)",
                static_cast<i32>(res));
    return ComputePipeline{};
  }

  VulkanPipelineData* pd = AllocObject<VulkanPipelineData>();
  pd->Layout             = layout;
  pd->Pipeline           = pipeline;
  pd->DescSetLayout      = dsl;
  pd->NumSamplers        = numSamplers;
  pd->NumBindings        = numBindings;
  pd->IsCompute          = true;
  for (u32 i = 0; i < numSamplers; ++i)
    pd->Samplers[i] = samplers[i];
  for (u32 i = 0; i < numBindings; ++i)
    pd->BindingTypes[i] = bindingTypes[i];

  ComputePipeline cp;
  cp.Desc = desc;
  VulkanDevice* dev = this;
  cp.Data = Shared<void>(pd, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanPipelineData*>(p);
    if (d->Pipeline != VK_NULL_HANDLE)
      vkDestroyPipeline(dev->m_Device, d->Pipeline, nullptr);
    if (d->Layout != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(dev->m_Device, d->Layout, nullptr);
    if (d->DescSetLayout != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(dev->m_Device, d->DescSetLayout, nullptr);
    for (u32 i = 0; i < d->NumSamplers; ++i)
      if (d->Samplers[i] != VK_NULL_HANDLE)
        vkDestroySampler(dev->m_Device, d->Samplers[i], nullptr);
    FreeObject(d);
  });
  return cp;
}

// ─────────────────────────────────────────────────────────────────────────
// Uploads
// ─────────────────────────────────────────────────────────────────────────

void VulkanDevice::OneTimeSubmit(void (*record)(VkCommandBuffer, void*),
                                  void* ctx) noexcept
{
  VkCommandBufferAllocateInfo cbai{};
  cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbai.commandPool        = m_GraphicsCommandPool;
  cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbai.commandBufferCount = 1;

  VkCommandBuffer cb = VK_NULL_HANDLE;
  vkAllocateCommandBuffers(m_Device, &cbai, &cb);

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cb, &bi);
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

void VulkanDevice::UploadTextureData(Texture& texture,
                                      ::std::span<const ::gecko::byte> data,
                                      u32 mip, u32 slice) noexcept
{
  if (!texture.IsValid() || data.empty())
    return;
  auto* td = static_cast<VulkanTextureData*>(texture.Data.get());

  // Staging buffer with mapped CPU memory.
  VkBufferCreateInfo bci{};
  bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bci.size        = data.size();
  bci.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo aci{};
  aci.usage = VMA_MEMORY_USAGE_CPU_ONLY;
  aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
            | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBuffer          staging      = VK_NULL_HANDLE;
  VmaAllocation     stagingAlloc = nullptr;
  VmaAllocationInfo info{};
  if (vmaCreateBuffer(m_Allocator, &bci, &aci, &staging, &stagingAlloc, &info)
      != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice::UploadTextureData staging allocation failed");
    return;
  }
  ::std::memcpy(info.pMappedData, data.data(), data.size());

  struct Ctx
  {
    VulkanTextureData* TD;
    VkBuffer           Src;
    u32                Mip;
    u32                Slice;
    VkImageLayout      OldLayout;
  } ctx{td, staging, mip, slice, td->CurrentLayout};

  OneTimeSubmit(
      [](VkCommandBuffer cb, void* c) {
        auto* x = static_cast<Ctx*>(c);

        auto transition = [&](VkImageLayout oldL, VkImageLayout newL,
                               VkAccessFlags srcA, VkAccessFlags dstA,
                               VkPipelineStageFlags srcS,
                               VkPipelineStageFlags dstS) {
          VkImageMemoryBarrier b{};
          b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
          b.oldLayout           = oldL;
          b.newLayout           = newL;
          b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          b.image               = x->TD->Image;
          b.subresourceRange.aspectMask = x->TD->Aspect;
          b.subresourceRange.baseMipLevel   = x->Mip;
          b.subresourceRange.levelCount     = 1;
          b.subresourceRange.baseArrayLayer = x->Slice;
          b.subresourceRange.layerCount     = 1;
          b.srcAccessMask = srcA;
          b.dstAccessMask = dstA;
          vkCmdPipelineBarrier(cb, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
        };

        transition(VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferImageCopy cp{};
        cp.imageSubresource.aspectMask     = x->TD->Aspect;
        cp.imageSubresource.mipLevel       = x->Mip;
        cp.imageSubresource.baseArrayLayer = x->Slice;
        cp.imageSubresource.layerCount     = 1;
        cp.imageExtent = {x->TD->Width, x->TD->Height, 1};
        vkCmdCopyBufferToImage(cb, x->Src, x->TD->Image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);

        transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
      },
      &ctx);

  td->CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vmaDestroyBuffer(m_Allocator, staging, stagingAlloc);
}

void VulkanDevice::UploadBufferData(Buffer& buffer,
                                     ::std::span<const ::gecko::byte> data,
                                     u32 offset) noexcept
{
  if (!buffer.IsValid() || data.empty())
    return;
  auto* bd = static_cast<VulkanBufferData*>(buffer.Data.get());

  // Create a staging buffer
  VkBufferCreateInfo bci{};
  bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bci.size  = data.size();
  bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo aci{};
  aci.usage         = VMA_MEMORY_USAGE_CPU_ONLY;
  aci.flags         = VMA_ALLOCATION_CREATE_MAPPED_BIT
               | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBuffer          staging = VK_NULL_HANDLE;
  VmaAllocation     stagingAlloc = nullptr;
  VmaAllocationInfo info{};
  if (vmaCreateBuffer(m_Allocator, &bci, &aci, &staging, &stagingAlloc, &info)
      != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice::UploadBufferData staging allocation failed");
    return;
  }

  ::std::memcpy(info.pMappedData, data.data(), data.size());

  struct Ctx
  {
    VkBuffer     src;
    VkBuffer     dst;
    VkDeviceSize size;
    u32          dstOffset;
  } ctx{staging, bd->Buffer, data.size(), offset};

  OneTimeSubmit(
      [](VkCommandBuffer cb, void* c) {
        auto*        x = static_cast<Ctx*>(c);
        VkBufferCopy cp{};
        cp.dstOffset = x->dstOffset;
        cp.size      = x->size;
        vkCmdCopyBuffer(cb, x->src, x->dst, 1, &cp);
      },
      &ctx);

  vmaDestroyBuffer(m_Allocator, staging, stagingAlloc);
}

}  // namespace gecko::graphics
