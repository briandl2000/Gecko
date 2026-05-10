#if defined(GECKO_GRAPHICS_VULKAN)
#define VMA_IMPLEMENTATION 1
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

// VMA's heavy use of partial C-style initializers trips our -Werror set.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include "vulkan_device.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"
#include "vulkan_command_list.h"
#include "vulkan_gpu_sampler.h"
#include "vulkan_surface.h"
#include "vulkan_util.h"

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace gecko::graphics {

// -- Small allocation helpers (route through Gecko allocator) -------------

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

// -- Debug messenger callback ---------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT /*types*/,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                    void* /*userdata*/)
{
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    GECKO_ERROR(labels::Vulkan, "%s", data->pMessage);
  else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    GECKO_WARN(labels::Vulkan, "%s", data->pMessage);
  return VK_FALSE;
}

// ------------------------------------------------------------
// VulkanDevice construction / teardown
// ------------------------------------------------------------

VulkanDevice::VulkanDevice(const GraphicsDeviceDesc& desc) noexcept
{
  GECKO_PROFILE_NAMED(labels::Vulkan, "VulkanDevice::Ctor");

  // -- Instance --------------------------------------------------

  VkApplicationInfo appInfo {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = desc.AppName;
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
  appInfo.pEngineName = "Gecko";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  auto surfaceExts = GetRequiredSurfaceExtensions();

  // -- Enumerate available layers / extensions (for graceful fallback) --
  ::std::vector<VkExtensionProperties> availableExts;
  {
    GECKO_PROFILE_NAMED(labels::Vulkan, "vkEnumerateInstanceExtensionProperties");
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
      GECKO_WARN(labels::Vulkan, "VulkanDevice: instance extension '%s' unavailable", e);
  }

  ::std::vector<VkLayerProperties> availableLayers;
  {
    GECKO_PROFILE_NAMED(labels::Vulkan, "vkEnumerateInstanceLayerProperties");
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
    GECKO_WARN(labels::Vulkan, "VulkanDevice: VK_EXT_debug_utils not available; "
                               "debug messenger disabled");

  ::std::vector<const char*> layers;
  if (desc.Debug && hasLayer("VK_LAYER_KHRONOS_validation"))
    layers.push_back("VK_LAYER_KHRONOS_validation");
  else if (desc.Debug)
    GECKO_WARN(labels::Vulkan, "VulkanDevice: VK_LAYER_KHRONOS_validation not available; "
                               "validation disabled");

  VkInstanceCreateInfo instanceCreateInfo {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pApplicationInfo = &appInfo;
  instanceCreateInfo.enabledExtensionCount = static_cast<u32>(instanceExts.size());
  instanceCreateInfo.ppEnabledExtensionNames = instanceExts.data();
  instanceCreateInfo.enabledLayerCount = static_cast<u32>(layers.size());
  instanceCreateInfo.ppEnabledLayerNames = layers.data();

  if ([&]() noexcept {
        GECKO_PROFILE_NAMED(labels::Vulkan, "vkCreateInstance");
        return vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance) != VK_SUCCESS;
      }())
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice: vkCreateInstance failed "
                "(requested %u extensions, %u layers)",
                instanceCreateInfo.enabledExtensionCount, instanceCreateInfo.enabledLayerCount);
    for (u32 i = 0; i < instanceCreateInfo.enabledExtensionCount; ++i)
      GECKO_ERROR(labels::Vulkan, "  ext: %s", instanceExts[i]);
    for (u32 i = 0; i < instanceCreateInfo.enabledLayerCount; ++i)
      GECKO_ERROR(labels::Vulkan, "  layer: %s", layers[i]);
    return;
  }

  // Debug messenger
  if (haveDebugUtils)
  {
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo {};
    debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugMessengerCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugMessengerCreateInfo.pfnUserCallback = DebugCallback;
    auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
    if (createFn != nullptr)
      createFn(m_Instance, &debugMessengerCreateInfo, nullptr, &m_DebugMessenger);
  }

  m_HasDebugUtils = haveDebugUtils;

  // -- Physical device -------------------------------------------

  u32 physicalDeviceCount = 0;
  {
    GECKO_PROFILE_NAMED(labels::Vulkan, "vkEnumeratePhysicalDevices");
    vkEnumeratePhysicalDevices(m_Instance, &physicalDeviceCount, nullptr);
  }
  if (physicalDeviceCount == 0)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: no Vulkan physical devices");
    return;
  }
  ::std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
  vkEnumeratePhysicalDevices(m_Instance, &physicalDeviceCount, physicalDevices.data());

  // Prefer discrete GPU
  m_PhysicalDevice = physicalDevices[0];
  for (auto& candidate : physicalDevices)
  {
    VkPhysicalDeviceProperties candidateProps {};
    vkGetPhysicalDeviceProperties(candidate, &candidateProps);
    if (candidateProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
      m_PhysicalDevice = candidate;
      break;
    }
  }

  VkPhysicalDeviceProperties physicalDeviceProps {};
  vkGetPhysicalDeviceProperties(m_PhysicalDevice, &physicalDeviceProps);
  GECKO_INFO(labels::Vulkan, "VulkanDevice: using GPU '%s'", physicalDeviceProps.deviceName);

  m_TimestampPeriodNs = physicalDeviceProps.limits.timestampPeriod;

  // Find graphics queue family
  u32 queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
  ::std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());
  m_GraphicsQueueFamily = UINT32_MAX;
  for (u32 i = 0; i < queueFamilyCount; ++i)
  {
    if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
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

  // -- Logical device --------------------------------------------

  f32 queuePriority = 1.0F;
  VkDeviceQueueCreateInfo queueCreateInfo {};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  ::std::vector<const char*> deviceExts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  // Query supported features so we only enable what the physical device
  // actually exposes.
  VkPhysicalDeviceVulkan13Features supported13 {};
  supported13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  VkPhysicalDeviceVulkan12Features supported12 {};
  supported12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  supported12.pNext = &supported13;
  VkPhysicalDeviceFeatures2 supported2 {};
  supported2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  supported2.pNext = &supported12;
  {
    GECKO_PROFILE_NAMED(labels::Vulkan, "vkGetPhysicalDeviceFeatures2");
    vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &supported2);
  }

  if (supported13.dynamicRendering != VK_TRUE || supported13.synchronization2 != VK_TRUE)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanDevice: required features missing (dynamicRendering=%d, "
                "synchronization2=%d)",
                supported13.dynamicRendering, supported13.synchronization2);
    return;
  }

  VkPhysicalDeviceVulkan13Features features13 {};
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features13.dynamicRendering = VK_TRUE;
  features13.synchronization2 = VK_TRUE;

  VkPhysicalDeviceVulkan12Features features12 {};
  features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  // hostQueryReset is optional -- only enable when supported, otherwise
  // fall back to cmdResetQueryPool (see CreateTimestampQueryPool).
  m_HasHostQueryReset = (supported12.hostQueryReset == VK_TRUE);
  features12.hostQueryReset = m_HasHostQueryReset ? VK_TRUE : VK_FALSE;
  features12.pNext = &features13;

  VkPhysicalDeviceFeatures2 features2 {};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features2.pNext = &features12;

  VkDeviceCreateInfo deviceCreateInfo {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.pNext = &features2;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
  deviceCreateInfo.enabledExtensionCount = static_cast<u32>(deviceExts.size());
  deviceCreateInfo.ppEnabledExtensionNames = deviceExts.data();

  if ([&]() noexcept {
        GECKO_PROFILE_NAMED(labels::Vulkan, "vkCreateDevice");
        return vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device);
      }() != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: vkCreateDevice failed");
    return;
  }

  vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
  m_PresentQueue = m_GraphicsQueue;

  // -- Command pool ----------------------------------------------

  VkCommandPoolCreateInfo cmdPoolCreateInfo {};
  cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cmdPoolCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
  {
    GECKO_PROFILE_NAMED(labels::Vulkan, "vkCreateCommandPool");
    VULKAN_CHECK(vkCreateCommandPool(m_Device, &cmdPoolCreateInfo, nullptr, &m_GraphicsCommandPool));
  }

  // -- VMA allocator ---------------------------------------------

  VmaVulkanFunctions vulkanFunctions {};
  vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocatorCreateInfo {};
  allocatorCreateInfo.instance = m_Instance;
  allocatorCreateInfo.physicalDevice = m_PhysicalDevice;
  allocatorCreateInfo.device = m_Device;
  allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
  {
    GECKO_PROFILE_NAMED(labels::Vulkan, "vmaCreateAllocator");
    VULKAN_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &m_Allocator));
  }

  // -- Descriptor pool -------------------------------------------
  // Example-grade: a single large pool, never reset. Sufficient for the
  // current example (one or two BindTexture calls per frame, short-lived).
  // Production code should switch to per-frame ring pools.
  {
    VkDescriptorPoolSize poolSizes[1] {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 4096;

    VkDescriptorPoolCreateInfo descPoolCreateInfo {};
    descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descPoolCreateInfo.maxSets = 4096;
    descPoolCreateInfo.poolSizeCount = 1;
    descPoolCreateInfo.pPoolSizes = poolSizes;
    GECKO_PROFILE_NAMED(labels::Vulkan, "vkCreateDescriptorPool");
    VULKAN_CHECK(vkCreateDescriptorPool(m_Device, &descPoolCreateInfo, nullptr, &m_DescriptorPool));
  }

  m_Valid = true;
  GECKO_INFO(labels::Vulkan, "VulkanDevice ready");
}

VulkanDevice::~VulkanDevice()
{
  if (m_Device != VK_NULL_HANDLE)
    vkDeviceWaitIdle(m_Device);

  // GPU is idle -- drain deferred command lists and the tracker-fence pool.
  DrainPending();
  for (VkFence f : m_FreeFences)
  {
    if (f != VK_NULL_HANDLE)
      vkDestroyFence(m_Device, f, nullptr);
  }
  m_FreeFences.clear();

  if (m_DescriptorPool != VK_NULL_HANDLE)
    vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

  if (m_Allocator != VK_NULL_HANDLE)
    vmaDestroyAllocator(m_Allocator);

  if (m_GraphicsCommandPool != VK_NULL_HANDLE)
    vkDestroyCommandPool(m_Device, m_GraphicsCommandPool, nullptr);

  // Destroy any lazily-created per-thread command pools.
  {
    ::std::lock_guard<::std::mutex> lock(m_ThreadPoolsMutex);
    for (auto& kv : m_ThreadPools)
    {
      if (kv.second != VK_NULL_HANDLE)
        vkDestroyCommandPool(m_Device, kv.second, nullptr);
    }
    m_ThreadPools.clear();
  }

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

VkCommandPool VulkanDevice::AcquireThreadCommandPool() noexcept
{
  const ::std::thread::id tid = ::std::this_thread::get_id();
  {
    ::std::lock_guard<::std::mutex> lock(m_ThreadPoolsMutex);
    auto it = m_ThreadPools.find(tid);
    if (it != m_ThreadPools.end())
      return it->second;
  }

  VkCommandPoolCreateInfo cmdPoolCreateInfo {};
  cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cmdPoolCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;

  VkCommandPool pool = VK_NULL_HANDLE;
  if (vkCreateCommandPool(m_Device, &cmdPoolCreateInfo, nullptr, &pool) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::AcquireThreadCommandPool: vkCreateCommandPool "
                                "failed");
    return VK_NULL_HANDLE;
  }

  ::std::lock_guard<::std::mutex> lock(m_ThreadPoolsMutex);
  m_ThreadPools[tid] = pool;
  return pool;
}

void VulkanDevice::WaitIdleLocked() noexcept
{
  if (m_Device == VK_NULL_HANDLE)
    return;
  ::std::lock_guard<::std::mutex> lock(m_QueueMutex);
  vkDeviceWaitIdle(m_Device);
}

VkFence VulkanDevice::AcquireTrackerFence() noexcept
{
  {
    ::std::lock_guard<::std::mutex> lock(m_PendingMutex);
    if (!m_FreeFences.empty())
    {
      VkFence f = m_FreeFences.back();
      m_FreeFences.pop_back();
      vkResetFences(m_Device, 1, &f);
      return f;
    }
  }
  VkFenceCreateInfo fenceCreateInfo {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence f = VK_NULL_HANDLE;
  if (vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &f) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::AcquireTrackerFence: vkCreateFence failed");
    return VK_NULL_HANDLE;
  }
  return f;
}

void VulkanDevice::ReleaseTrackerFence(VkFence fence) noexcept
{
  if (fence == VK_NULL_HANDLE)
    return;
  ::std::lock_guard<::std::mutex> lock(m_PendingMutex);
  m_FreeFences.push_back(fence);
}

void VulkanDevice::ReapPending() noexcept
{
  ::std::vector<PendingSubmit> completed;
  {
    ::std::lock_guard<::std::mutex> lock(m_PendingMutex);
    for (auto it = m_Pending.begin(); it != m_Pending.end();)
    {
      if (it->Fence != VK_NULL_HANDLE && vkGetFenceStatus(m_Device, it->Fence) == VK_SUCCESS)
      {
        completed.emplace_back(::std::move(*it));
        it = m_Pending.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }
  // Recycle fences and destroy the command lists outside the mutex so
  // ~VulkanCommandList can't deadlock on device-side state.
  for (auto& entry : completed)
  {
    entry.Cmd.reset();
    ReleaseTrackerFence(entry.Fence);
  }
}

void VulkanDevice::DrainPending() noexcept
{
  // Caller must have already ensured the GPU is idle (vkDeviceWaitIdle).
  ::std::vector<PendingSubmit> pending;
  {
    ::std::lock_guard<::std::mutex> lock(m_PendingMutex);
    pending.swap(m_Pending);
  }
  for (auto& entry : pending)
  {
    entry.Cmd.reset();
    if (entry.Fence != VK_NULL_HANDLE)
      vkDestroyFence(m_Device, entry.Fence, nullptr);
  }
}

// ------------------------------------------------------------
// Swapchain
// ------------------------------------------------------------

bool VulkanDevice::BuildSwapchainResources(VulkanSwapchainData& data, VkSwapchainKHR oldSC) noexcept
{
  // Query surface caps
  VkSurfaceCapabilitiesKHR surfaceCaps {};
  VULKAN_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, data.Surface, &surfaceCaps));

  // Extent
  VkExtent2D extent {};
  if (surfaceCaps.currentExtent.width != UINT32_MAX)
  {
    extent = surfaceCaps.currentExtent;
  }
  else
  {
    extent.width = data.Desc.Width;
    extent.height = data.Desc.Height;
  }
  if (extent.width == 0 || extent.height == 0)
  {
    GECKO_WARN(labels::Vulkan, "VulkanDevice: zero-size surface, deferring swapchain build");
    return false;
  }
  data.Extent = extent;

  // Format
  u32 formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, data.Surface, &formatCount, nullptr);
  if (formatCount == 0)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: surface reports zero supported formats");
    return false;
  }
  ::std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, data.Surface, &formatCount, surfaceFormats.data());

  VkFormat wanted = ToVkFormat(data.Desc.Format);
  VkSurfaceFormatKHR chosenFormat = surfaceFormats[0];
  for (auto& candidate : surfaceFormats)
  {
    if (candidate.format == wanted && candidate.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
    {
      chosenFormat = candidate;
      break;
    }
  }
  data.Format = chosenFormat.format;

  // Present mode -- FIFO is guaranteed; only pick IMMEDIATE if supported.
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
  if (!data.Desc.VSync)
  {
    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, data.Surface, &presentModeCount, nullptr);
    ::std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, data.Surface, &presentModeCount, presentModes.data());
    for (VkPresentModeKHR m : presentModes)
    {
      if (m == VK_PRESENT_MODE_IMMEDIATE_KHR)
      {
        presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        break;
      }
    }
  }

  // Image count
  u32 imageCount = data.Desc.NumBackBuffers;
  if (imageCount < surfaceCaps.minImageCount)
    imageCount = surfaceCaps.minImageCount;
  if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount)
    imageCount = surfaceCaps.maxImageCount;
  if (imageCount > MaxSwapchainImages)
    imageCount = MaxSwapchainImages;

  VkSwapchainCreateInfoKHR swapchainCreateInfo {};
  swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainCreateInfo.surface = data.Surface;
  swapchainCreateInfo.minImageCount = imageCount;
  swapchainCreateInfo.imageFormat = chosenFormat.format;
  swapchainCreateInfo.imageColorSpace = chosenFormat.colorSpace;
  swapchainCreateInfo.imageExtent = extent;
  swapchainCreateInfo.imageArrayLayers = 1;
  swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainCreateInfo.preTransform = surfaceCaps.currentTransform;
  swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainCreateInfo.presentMode = presentMode;
  swapchainCreateInfo.clipped = VK_TRUE;
  swapchainCreateInfo.oldSwapchain = oldSC;

  if (vkCreateSwapchainKHR(m_Device, &swapchainCreateInfo, nullptr, &data.Swapchain) != VK_SUCCESS)
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
    data.ImageLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageViewCreateInfo viewCreateInfo {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = data.Images[i];
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = chosenFormat.format;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.layerCount = 1;
    VULKAN_CHECK(vkCreateImageView(m_Device, &viewCreateInfo, nullptr, &data.ImageViews[i]));
  }

  // Sync objects.
  // ImageAvailable + InFlight are per-frame-in-flight. RenderFinished is
  // per-image: the present engine may still be holding the semaphore by
  // the time the frame slot recycles, so it can't be reused across frames.
  if (data.InFlight[0] == VK_NULL_HANDLE)
  {
    VkSemaphoreCreateInfo semaphoreCreateInfo {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceCreateInfo {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (u32 i = 0; i < MaxFramesInFlight; ++i)
    {
      VULKAN_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &data.ImageAvailable[i]));
      VULKAN_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &data.InFlight[i]));
    }
  }
  {
    VkSemaphoreCreateInfo semaphoreCreateInfo {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (u32 i = 0; i < data.ImageCount; ++i)
    {
      if (data.RenderFinished[i] == VK_NULL_HANDLE)
        VULKAN_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &data.RenderFinished[i]));
    }
  }

  return true;
}

void VulkanDevice::DestroySwapchainResources(VulkanSwapchainData& data, bool destroySurface) noexcept
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
      if (data.InFlight[i] != VK_NULL_HANDLE)
        vkDestroyFence(m_Device, data.InFlight[i], nullptr);
      data.ImageAvailable[i] = VK_NULL_HANDLE;
      data.InFlight[i] = VK_NULL_HANDLE;
    }
    for (u32 i = 0; i < MaxSwapchainImages; ++i)
    {
      if (data.RenderFinished[i] != VK_NULL_HANDLE)
      {
        vkDestroySemaphore(m_Device, data.RenderFinished[i], nullptr);
        data.RenderFinished[i] = VK_NULL_HANDLE;
      }
    }
    if (data.Surface != VK_NULL_HANDLE)
    {
      vkDestroySurfaceKHR(m_Instance, data.Surface, nullptr);
      data.Surface = VK_NULL_HANDLE;
    }
  }
}

Swapchain VulkanDevice::CreateSwapchain(const ::gecko::platform::NativeWindowHandle& native,
                                        const SwapchainDesc& desc) noexcept
{
  GECKO_PROFILE_NAMED(labels::Vulkan, "VulkanDevice::CreateSwapchain");

  if (!m_Valid)
    return Swapchain {};

  VulkanSwapchainData* data = AllocObject<VulkanSwapchainData>();
  if (data == nullptr)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::CreateSwapchain: allocation failed");
    return Swapchain {};
  }
  data->Native = native;
  data->Desc = desc;

  if (CreateSurface(m_Instance, native, &data->Surface) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: surface creation failed");
    FreeObject(data);
    return Swapchain {};
  }

  VkBool32 support = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, m_GraphicsQueueFamily, data->Surface, &support);
  if (support == VK_FALSE)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: queue family cannot present to surface");
    DestroySwapchainResources(*data, true);
    FreeObject(data);
    return Swapchain {};
  }

  if (!BuildSwapchainResources(*data, VK_NULL_HANDLE))
  {
    DestroySwapchainResources(*data, true);
    FreeObject(data);
    return Swapchain {};
  }

  Swapchain sc;
  sc.Desc = desc;
  sc.Desc.Width = data->Extent.width;
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
  swapchain = Swapchain {};
}

void VulkanDevice::ResizeSwapchain(Swapchain& swapchain) noexcept
{
  GECKO_PROFILE_NAMED(labels::Vulkan, "VulkanDevice::ResizeSwapchain");

  if (!swapchain.Data)
    return;
  auto* data = static_cast<VulkanSwapchainData*>(swapchain.Data.get());
  // Sync user-visible desc back into internal data so the Wayland fallback
  // path (where VkSurfaceCapabilities.currentExtent is always UINT32_MAX)
  // sees the new requested size.  On X11/Win32 currentExtent has real
  // values and this is a no-op.
  data->Desc.Width = swapchain.Desc.Width;
  data->Desc.Height = swapchain.Desc.Height;
  vkDeviceWaitIdle(m_Device);
  VkSwapchainKHR old = data->Swapchain;
  data->Swapchain = VK_NULL_HANDLE;
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

  swapchain.Desc.Width = data->Extent.width;
  swapchain.Desc.Height = data->Extent.height;
}

FrameContext VulkanDevice::BeginFrame(Swapchain& swapchain) noexcept
{
  GECKO_PROFILE_NAMED(labels::Vulkan, "VulkanDevice::BeginFrame");

  // Reclaim completed command lists from previous submits before doing
  // anything else this frame.
  ReapPending();

  FrameContext ctx {};
  if (!swapchain.Data)
    return ctx;
  auto* data = static_cast<VulkanSwapchainData*>(swapchain.Data.get());
  if (data->Swapchain == VK_NULL_HANDLE)
    return ctx;

  const u32 frame = data->FrameIndex;
  {
    GECKO_PROFILE_NAMED(labels::Vulkan, "vkWaitForFences");
    vkWaitForFences(m_Device, 1, &data->InFlight[frame], VK_TRUE, UINT64_MAX);
  }

  u32 imageIndex = 0;
  VkResult ar = VK_SUCCESS;
  {
    GECKO_PROFILE_NAMED(labels::Vulkan, "vkAcquireNextImageKHR");
    ar = vkAcquireNextImageKHR(m_Device, data->Swapchain, UINT64_MAX, data->ImageAvailable[frame], VK_NULL_HANDLE,
                               &imageIndex);
  }
  if (ar == VK_ERROR_OUT_OF_DATE_KHR)
  {
    ResizeSwapchain(swapchain);
    return ctx;  // skip this frame
  }
  if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: acquire failed (%d)", static_cast<i32>(ar));
    return ctx;
  }

  vkResetFences(m_Device, 1, &data->InFlight[frame]);

  data->AcquiredIndex = imageIndex;

  // Build a RenderTarget wrapper around the acquired image.
  VulkanRTData* rtd = AllocObject<VulkanRTData>();
  if (rtd == nullptr)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::BeginFrame: allocation failed");
    return ctx;
  }
  rtd->RTKind = VulkanRTData::Kind::Swapchain;
  rtd->Image = data->Images[imageIndex];
  rtd->ImageView = data->ImageViews[imageIndex];
  rtd->SwapchainData = data;
  rtd->FrameIndex = frame;
  rtd->ImageIndex = imageIndex;

  RenderTarget rt;
  rt.Desc.Width = data->Extent.width;
  rt.Desc.Height = data->Extent.height;
  rt.Desc.NumRenderTargets = 1;
  rt.Desc.RenderTargetFormats[0] = FromVkFormat(data->Format);
  rt.Data = Shared<void>(rtd, [](void* p) noexcept { FreeObject(static_cast<VulkanRTData*>(p)); });

  ctx.SC = &swapchain;
  ctx.FrameIndex = frame;
  ctx.ImageIndex = imageIndex;
  ctx.BackBuffer = ::std::move(rt);
  ctx.Valid = true;
  return ctx;
}

void VulkanDevice::Present(::std::span<const FrameContext> frames) noexcept
{
  GECKO_PROFILE_ALWAYS_NAMED(labels::Vulkan, "VulkanDevice::Present");

  if (frames.empty())
    return;

  VkSwapchainKHR scs[MaxSwapchainsPerSubmit] {};
  u32 indices[MaxSwapchainsPerSubmit] {};
  VkSemaphore waits[MaxSwapchainsPerSubmit] {};
  u32 count = 0;

  for (const auto& f : frames)
  {
    if (!f.Valid || f.SC == nullptr || !f.SC->Data)
      continue;
    if (count >= MaxSwapchainsPerSubmit)
    {
      GECKO_WARN(labels::Vulkan, "VulkanDevice::Present: more than %u swapchains, truncated", MaxSwapchainsPerSubmit);
      break;
    }
    auto* data = static_cast<VulkanSwapchainData*>(f.SC->Data.get());
    scs[count] = data->Swapchain;
    indices[count] = f.ImageIndex;
    waits[count] = data->RenderFinished[f.ImageIndex];
    ++count;
  }

  if (count == 0)
    return;

  VkPresentInfoKHR presentInfo {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = count;
  presentInfo.pWaitSemaphores = waits;
  presentInfo.swapchainCount = count;
  presentInfo.pSwapchains = scs;
  presentInfo.pImageIndices = indices;

  VkResult presentResult;
  {
    GECKO_PROFILE_NAMED(labels::Vulkan, "Present::QueueLock");
    ::std::lock_guard<::std::mutex> lock(m_QueueMutex);
    GECKO_PROFILE_NAMED(labels::Vulkan, "vkQueuePresentKHR");
    presentResult = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
  }
  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
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
    auto* data = static_cast<VulkanSwapchainData*>(f.SC->Data.get());
    data->FrameIndex = (data->FrameIndex + 1) % MaxFramesInFlight;
  }
}

// ------------------------------------------------------------
// Command lists
// ------------------------------------------------------------

Unique<ICommandList> VulkanDevice::CreateGraphicsCommandList() noexcept
{
  return CreateUnique<VulkanCommandList>(*this, /*compute*/ false);
}

Unique<ICommandList> VulkanDevice::CreateComputeCommandList() noexcept
{
  return CreateUnique<VulkanCommandList>(*this, /*compute*/ true);
}

void VulkanDevice::ExecuteGraphicsCommandList(Unique<ICommandList> commandList) noexcept
{
  GECKO_PROFILE_ALWAYS_NAMED(labels::Vulkan, "VulkanDevice::ExecuteGraphicsCommandList");

  auto* cl = static_cast<VulkanCommandList*>(commandList.get());
  if (cl == nullptr || !cl->IsValid())
    return;

  VkCommandBuffer cmdBuf = cl->CommandBuffer();

  // Gather wait/signal semaphores from touched swapchains
  constexpr u32 MaxSems = MaxSwapchainsPerSubmit;
  VkSemaphore waitSems[MaxSems] {};
  VkSemaphore sigSems[MaxSems] {};
  VkPipelineStageFlags waitStages[MaxSems] {};
  u32 waitCount = 0;
  u32 sigCount = 0;

  auto touched = cl->TouchedSwapchains();
  for (u32 i = 0; i < touched.Count && i < MaxSems; ++i)
  {
    auto* data = touched.Data[i].Data;
    const u32 frameIdx = touched.Data[i].FrameIndex;
    const u32 imageIdx = touched.Data[i].ImageIndex;
    waitSems[waitCount] = data->ImageAvailable[frameIdx];
    waitStages[waitCount] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    ++waitCount;
    sigSems[sigCount] = data->RenderFinished[imageIdx];
    ++sigCount;
  }

  VkSubmitInfo submitInfo {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuf;
  submitInfo.waitSemaphoreCount = waitCount;
  submitInfo.pWaitSemaphores = waitSems;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.signalSemaphoreCount = sigCount;
  submitInfo.pSignalSemaphores = sigSems;

  // Signal the first touched swapchain's InFlight fence with the real
  // submit. Every other touched swapchain needs its own fence signaled too,
  // otherwise BeginFrame will block forever after `MaxFramesInFlight` cycles
  // (its per-swapchain vkWaitForFences would never complete). Use cheap
  // empty signaling submits for the remaining fences.
  VkFence primaryFence = VK_NULL_HANDLE;
  if (touched.Count >= 1)
  {
    auto* d0 = touched.Data[0].Data;
    primaryFence = d0->InFlight[touched.Data[0].FrameIndex];
  }

  // Acquire a tracker fence so we can safely defer destruction of this
  // command list until the GPU is done with it -- no vkDeviceWaitIdle needed.
  VkFence trackerFence = AcquireTrackerFence();
  const bool useTrackerAsPrimary = (primaryFence == VK_NULL_HANDLE);
  if (useTrackerAsPrimary)
    primaryFence = trackerFence;

  {
    GECKO_PROFILE_NAMED(labels::Vulkan, "ExecuteGraphics::QueueLock");
    ::std::lock_guard<::std::mutex> lock(m_QueueMutex);
    {
      GECKO_PROFILE_NAMED(labels::Vulkan, "vkQueueSubmit");
      // Notify any attached GPU sampler that the cmd list is about to
      // be submitted. The sampler uses the first such CPU timestamp
      // per frame as its rebase anchor so GPU zones line up with the
      // vkQueueSubmit call on the CPU timeline.
      if (auto* sampler = cl->GetAttachedGpuSampler(); sampler != nullptr)
      {
        if (auto* p = ::gecko::GetProfiler(); p != nullptr)
          sampler->OnSubmit(p->NowNs());
      }
      VULKAN_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, primaryFence));
    }

    for (u32 i = 1; i < touched.Count; ++i)
    {
      auto* d = touched.Data[i].Data;
      VkFence f = d->InFlight[touched.Data[i].FrameIndex];
      VkSubmitInfo empty {};
      empty.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      VULKAN_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &empty, f));
    }

    // If the real submit was already using a swapchain fence as primary,
    // we still need the tracker to fire -- push an empty submit for it.
    if (!useTrackerAsPrimary && trackerFence != VK_NULL_HANDLE)
    {
      VkSubmitInfo empty {};
      empty.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      VULKAN_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &empty, trackerFence));
    }
  }  // queue lock released

  // Defer destruction until trackerFence signals (reaped by BeginFrame).
  {
    ::std::lock_guard<::std::mutex> lock(m_PendingMutex);
    m_Pending.push_back({trackerFence, ::std::move(commandList)});
  }
}

void VulkanDevice::ExecuteComputeCommandList(Unique<ICommandList> commandList) noexcept
{
  ExecuteGraphicsCommandList(::std::move(commandList));
}

// ------------------------------------------------------------
// Resource creation (stubs for now -- triangle path doesn't need most)
// ------------------------------------------------------------

RenderTarget VulkanDevice::CreateRenderTarget(const RenderTargetDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return RenderTarget {};

  RenderTarget rt;
  rt.Desc = desc;

  auto* rtd = AllocObject<VulkanRTData>();
  rtd->RTKind = VulkanRTData::Kind::Offscreen;

  // -- Colour textures -------------------------------------------
  for (u32 i = 0; i < desc.NumRenderTargets; ++i)
  {
    TextureDesc td {};
    td.Width = desc.Width;
    td.Height = desc.Height;
    td.Depth = 1;
    td.NumMips = 1;
    td.NumArraySlices = 1;
    td.Format = desc.RenderTargetFormats[i];
    td.Type = TextureType::Tex2D;
    td.Memory = MemoryType::Dedicated;
    td.IsRenderTarget = true;
    td.OptimizedClear = desc.RenderTargetClearValues[i];

    Texture t = CreateTexture(td);
    if (!t.IsValid())
    {
      GECKO_ERROR(labels::Vulkan, "VulkanDevice::CreateRenderTarget: colour texture %u failed", i);
      FreeObject(rtd);
      return RenderTarget {};
    }

    auto* texData = static_cast<VulkanTextureData*>(t.Data.get());
    rtd->OffscreenTex[i] = texData;
    if (i == 0)
    {
      rtd->Image = texData->Image;
      rtd->ImageView = texData->ImageView;
    }
    rt.RenderTextures[i] = ::std::move(t);
  }
  rtd->NumOffscreen = desc.NumRenderTargets;

  // -- Depth texture (optional) ----------------------------------
  if (desc.DepthStencilFormat != DataFormat::None)
  {
    TextureDesc td {};
    td.Width = desc.Width;
    td.Height = desc.Height;
    td.Depth = 1;
    td.NumMips = 1;
    td.NumArraySlices = 1;
    td.Format = desc.DepthStencilFormat;
    td.Type = TextureType::Tex2D;
    td.Memory = MemoryType::Dedicated;
    td.IsDepthStencil = true;
    td.OptimizedClear = desc.DepthStencilClearValue;

    Texture d = CreateTexture(td);
    if (d.IsValid())
    {
      rtd->OffscreenDepth = static_cast<VulkanTextureData*>(d.Data.get());
      rt.DepthTexture = ::std::move(d);
    }
  }

  rt.Data = Shared<void>(rtd, [](void* p) noexcept { FreeObject(static_cast<VulkanRTData*>(p)); });
  return rt;
}

Buffer VulkanDevice::CreateVertexBuffer(const VertexBufferDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return Buffer {};

  const u32 size = desc.NumVertices * desc.VertexSize;

  VkBufferCreateInfo bufferCreateInfo {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = size;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo {};
  allocCreateInfo.usage = (desc.Memory == MemoryType::Shared) ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY;

  VulkanBufferData* bufferData = AllocObject<VulkanBufferData>();
  if (vmaCreateBuffer(m_Allocator, &bufferCreateInfo, &allocCreateInfo, &bufferData->Buffer, &bufferData->Allocation,
                      nullptr) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::CreateVertexBuffer vmaCreateBuffer failed");
    FreeObject(bufferData);
    return Buffer {};
  }

  Buffer b;
  b.Type = BufferType::Vertex;
  b.VertexDesc = desc;
  VulkanDevice* dev = this;
  b.Data = Shared<void>(bufferData, [dev](void* p) noexcept {
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
    return Buffer {};

  const u32 size = desc.NumIndices * sizeof(u32);

  VkBufferCreateInfo bufferCreateInfo {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = size;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo {};
  allocCreateInfo.usage = (desc.Memory == MemoryType::Shared) ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY;

  VulkanBufferData* bufferData = AllocObject<VulkanBufferData>();
  if (vmaCreateBuffer(m_Allocator, &bufferCreateInfo, &allocCreateInfo, &bufferData->Buffer, &bufferData->Allocation,
                      nullptr) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::CreateIndexBuffer vmaCreateBuffer failed");
    FreeObject(bufferData);
    return Buffer {};
  }

  Buffer b;
  b.Type = BufferType::Index;
  b.IndexDesc = desc;
  VulkanDevice* dev = this;
  b.Data = Shared<void>(bufferData, [dev](void* p) noexcept {
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
    return Buffer {};

  VkBufferCreateInfo bufferCreateInfo {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = desc.SizeInBytes;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo {};
  allocCreateInfo.usage = (desc.Memory == MemoryType::Shared) ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY;
  if (desc.Memory == MemoryType::Shared)
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VulkanBufferData* bufferData = AllocObject<VulkanBufferData>();
  if (vmaCreateBuffer(m_Allocator, &bufferCreateInfo, &allocCreateInfo, &bufferData->Buffer, &bufferData->Allocation,
                      nullptr) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::CreateConstantBuffer vmaCreateBuffer failed");
    FreeObject(bufferData);
    return Buffer {};
  }

  Buffer b;
  b.Type = BufferType::Constant;
  b.ConstantDesc = desc;
  VulkanDevice* dev = this;
  b.Data = Shared<void>(bufferData, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanBufferData*>(p);
    if (d->Buffer != VK_NULL_HANDLE)
      vmaDestroyBuffer(dev->m_Allocator, d->Buffer, d->Allocation);
    FreeObject(d);
  });
  return b;
}

Buffer VulkanDevice::CreateStructuredBuffer(const StructuredBufferDesc& desc) noexcept
{
  if (!desc.IsValid() || !m_Valid)
    return Buffer {};

  const VkDeviceSize size = static_cast<VkDeviceSize>(desc.NumElements) * desc.ElementSize;

  VkBufferCreateInfo bufferCreateInfo {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = size;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo {};
  allocCreateInfo.usage = (desc.Memory == MemoryType::Shared) ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY;
  if (desc.Memory == MemoryType::Shared)
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VulkanBufferData* bufferData = AllocObject<VulkanBufferData>();
  if (vmaCreateBuffer(m_Allocator, &bufferCreateInfo, &allocCreateInfo, &bufferData->Buffer, &bufferData->Allocation,
                      nullptr) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::CreateStructuredBuffer vmaCreateBuffer failed");
    FreeObject(bufferData);
    return Buffer {};
  }

  Buffer b;
  b.Type = BufferType::Structured;
  b.StructuredDesc = desc;
  VulkanDevice* dev = this;
  b.Data = Shared<void>(bufferData, [dev](void* p) noexcept {
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
    return Texture {};

  const VkFormat fmt = ToVkFormat(desc.Format);
  const bool isDepth = IsDepthFormat(desc.Format);

  VkImageCreateInfo imageCreateInfo {};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.format = fmt;
  imageCreateInfo.extent = {desc.Width, desc.Height, 1};
  imageCreateInfo.mipLevels = desc.NumMips > 0 ? desc.NumMips : 1;
  imageCreateInfo.arrayLayers = desc.NumArraySlices > 0 ? desc.NumArraySlices : 1;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.usage =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if (desc.IsRenderTarget)
    imageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (desc.IsDepthStencil || isDepth)
    imageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  if (desc.AllowUnorderedAccess)
    imageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo allocCreateInfo {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  auto* td = AllocObject<VulkanTextureData>();
  td->Format = fmt;
  td->Width = desc.Width;
  td->Height = desc.Height;
  td->IsRenderTarget = desc.IsRenderTarget || desc.IsDepthStencil;
  if (isDepth)
  {
    td->Aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (HasStencilComponent(desc.Format))
      td->Aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  else
  {
    td->Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  if (vmaCreateImage(m_Allocator, &imageCreateInfo, &allocCreateInfo, &td->Image, &td->Allocation, nullptr) !=
      VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::CreateTexture vmaCreateImage failed");
    FreeObject(td);
    return Texture {};
  }

  VkImageViewCreateInfo viewCreateInfo {};
  viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewCreateInfo.image = td->Image;
  viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewCreateInfo.format = fmt;
  viewCreateInfo.subresourceRange.aspectMask = td->Aspect;
  viewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;
  viewCreateInfo.subresourceRange.layerCount = imageCreateInfo.arrayLayers;
  if (vkCreateImageView(m_Device, &viewCreateInfo, nullptr, &td->ImageView) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::CreateTexture vkCreateImageView failed");
    vmaDestroyImage(m_Allocator, td->Image, td->Allocation);
    FreeObject(td);
    return Texture {};
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
  if (desc.DebugName != nullptr)
    SetObjectName(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<u64>(td->Image), desc.DebugName);
  return t;
}

// ------------------------------------------------------------
// Pipelines
// ------------------------------------------------------------

VkShaderModule VulkanDevice::CreateShaderModule(const ShaderCode& code) noexcept
{
  if (!code.IsValid())
    return VK_NULL_HANDLE;
  if (code.Format != ShaderFormat::SPIRV)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: only SPIRV shaders accepted (format=%d)", static_cast<i32>(code.Format));
    return VK_NULL_HANDLE;
  }

  // SPIR-V requires codeSize to be a multiple of 4 and pCode to be 4-byte
  // aligned. Copy into an aligned temporary if the caller's buffer isn't.
  const usize byteCount = code.Bytes.size();
  const auto* rawBytes = code.Bytes.data();
  if (byteCount == 0 || (byteCount % 4) != 0)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: SPIRV blob size %zu is not a multiple of 4 bytes", byteCount);
    return VK_NULL_HANDLE;
  }

  VkShaderModuleCreateInfo shaderModuleCreateInfo {};
  shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleCreateInfo.codeSize = byteCount;

  ::std::vector<u32> alignedCopy;
  const bool isAligned = (reinterpret_cast<::std::uintptr_t>(rawBytes) % alignof(u32)) == 0;
  if (isAligned)
  {
    shaderModuleCreateInfo.pCode = reinterpret_cast<const u32*>(rawBytes);
  }
  else
  {
    alignedCopy.resize(byteCount / sizeof(u32));
    ::std::memcpy(alignedCopy.data(), rawBytes, byteCount);
    shaderModuleCreateInfo.pCode = alignedCopy.data();
  }

  VkShaderModule m = VK_NULL_HANDLE;
  if (vkCreateShaderModule(m_Device, &shaderModuleCreateInfo, nullptr, &m) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: CreateShaderModule failed");
    return VK_NULL_HANDLE;
  }
  return m;
}

GraphicsPipeline VulkanDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) noexcept
{
  GECKO_PROFILE_NAMED(labels::Vulkan, "VulkanDevice::CreateGraphicsPipeline");

  if (!desc.IsValid() || !m_Valid)
    return GraphicsPipeline {};

  VkShaderModule vs = CreateShaderModule(desc.VertexShader);
  VkShaderModule ps = CreateShaderModule(desc.PixelShader);
  if (vs == VK_NULL_HANDLE)
    return GraphicsPipeline {};

  VkPipelineShaderStageCreateInfo stages[2] {};
  u32 stageCount = 0;

  stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[stageCount].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[stageCount].module = vs;
  stages[stageCount].pName = desc.VertexShader.Entry;
  ++stageCount;

  if (ps != VK_NULL_HANDLE)
  {
    stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[stageCount].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[stageCount].module = ps;
    stages[stageCount].pName = desc.PixelShader.Entry;
    ++stageCount;
  }

  // Vertex input
  VkVertexInputBindingDescription vertexBinding {};
  vertexBinding.binding = 0;
  vertexBinding.stride = desc.Layout.StrideInBytes;
  vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertexAttrs[VertexLayout::MaxAttributes] {};
  for (u32 i = 0; i < desc.Layout.NumAttributes; ++i)
  {
    vertexAttrs[i].location = i;
    vertexAttrs[i].binding = 0;
    vertexAttrs[i].format = ToVkFormat(desc.Layout.Attributes[i].AttributeFormat);
    vertexAttrs[i].offset = desc.Layout.Attributes[i].Offset;
  }

  VkPipelineVertexInputStateCreateInfo vertexInput {};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  if (desc.Layout.NumAttributes > 0)
  {
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &vertexBinding;
    vertexInput.vertexAttributeDescriptionCount = desc.Layout.NumAttributes;
    vertexInput.pVertexAttributeDescriptions = vertexAttrs;
  }

  VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology =
      (desc.Primitive == PrimitiveType::Lines) ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportState {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = (desc.Culling == CullMode::Back)    ? VK_CULL_MODE_BACK_BIT
                        : (desc.Culling == CullMode::Front) ? VK_CULL_MODE_FRONT_BIT
                                                            : VK_CULL_MODE_NONE;
  rasterizer.frontFace =
      (desc.Winding == WindingOrder::ClockWise) ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0F;

  VkPipelineMultisampleStateCreateInfo multisample {};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blendAttachments[RenderTargetDesc::MaxRenderTargets] {};
  for (u32 i = 0; i < desc.NumRenderTargets; ++i)
  {
    const auto& bs = desc.BlendStates[i];
    blendAttachments[i].blendEnable = bs.BlendEnable ? VK_TRUE : VK_FALSE;
    blendAttachments[i].srcColorBlendFactor = ToVkBlendFactor(bs.SrcColor);
    blendAttachments[i].dstColorBlendFactor = ToVkBlendFactor(bs.DstColor);
    blendAttachments[i].colorBlendOp = ToVkBlendOp(bs.ColorOp);
    blendAttachments[i].srcAlphaBlendFactor = ToVkBlendFactor(bs.SrcAlpha);
    blendAttachments[i].dstAlphaBlendFactor = ToVkBlendFactor(bs.DstAlpha);
    blendAttachments[i].alphaBlendOp = ToVkBlendOp(bs.AlphaOp);
    blendAttachments[i].colorWriteMask = ToVkColorWriteMask(bs.WriteMask);
  }

  VkPipelineColorBlendStateCreateInfo colorBlend {};
  colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlend.attachmentCount = desc.NumRenderTargets;
  colorBlend.pAttachments = blendAttachments;

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
  dynamicState.pDynamicStates = dynamicStates;

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

  // -- Descriptor set layout -------------------------------------
  // Resources are laid out as consecutive bindings in the order they
  // appear in `PipelineResources`, each `PipelineResource` contributing
  // `NumResources` individual descriptor slots of its type.
  VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
  VkDescriptorType bindingTypes[VulkanPipelineData::MaxBindings] {};
  u32 numBindings = 0;

  VkDescriptorSetLayoutBinding bindings[VulkanPipelineData::MaxBindings] {};

  for (u32 r = 0; r < desc.NumPipelineResources; ++r)
  {
    const auto& resource = desc.PipelineResources[r];
    const VkDescriptorType dtype = ToVkDescriptorType(resource.Type, /*isCompute*/ false);
    if (dtype == VK_DESCRIPTOR_TYPE_MAX_ENUM)
      continue;
    for (u32 n = 0; n < resource.NumResources && numBindings < VulkanPipelineData::MaxBindings; ++n)
    {
      const u32 slot = numBindings;
      bindings[slot].binding = slot;
      bindings[slot].descriptorType = dtype;
      bindings[slot].descriptorCount = 1;
      bindings[slot].stageFlags = (resource.ShaderVisibility == ShaderType::All)
                                      ? VkShaderStageFlags {VK_SHADER_STAGE_ALL_GRAPHICS}
                                      : ToVkShaderStage(resource.ShaderVisibility);
      bindingTypes[slot] = dtype;
      ++numBindings;
    }
  }

  if (numBindings > 0)
  {
    VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo {};
    descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descSetLayoutCreateInfo.bindingCount = numBindings;
    descSetLayoutCreateInfo.pBindings = bindings;
    VULKAN_CHECK(vkCreateDescriptorSetLayout(m_Device, &descSetLayoutCreateInfo, nullptr, &dsl));
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &dsl;
  }

  VkPushConstantRange pushConstantRange {};
  if (desc.PushConstantBytes > 0)
  {
    pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    pushConstantRange.offset = 0;
    pushConstantRange.size = desc.PushConstantBytes;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
  }

  VkPipelineLayout layout = VK_NULL_HANDLE;
  VULKAN_CHECK(vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &layout));

  // Dynamic rendering info (VK_KHR_dynamic_rendering core in 1.3)
  VkFormat colorFormats[RenderTargetDesc::MaxRenderTargets] {};
  for (u32 i = 0; i < desc.NumRenderTargets; ++i)
    colorFormats[i] = ToVkFormat(desc.RenderTargetFormats[i]);

  VkPipelineRenderingCreateInfo renderingCreateInfo {};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount = desc.NumRenderTargets;
  renderingCreateInfo.pColorAttachmentFormats = colorFormats;
  renderingCreateInfo.depthAttachmentFormat = ToVkFormat(desc.DepthStencilFormat);

  VkPipelineDepthStencilStateCreateInfo depthStencil {};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = desc.DepthStencil.DepthTestEnable ? VK_TRUE : VK_FALSE;
  depthStencil.depthWriteEnable = desc.DepthStencil.DepthWriteEnable ? VK_TRUE : VK_FALSE;
  depthStencil.depthCompareOp = ToVkCompareOp(desc.DepthStencil.DepthCompare);
  depthStencil.stencilTestEnable = desc.DepthStencil.StencilEnable ? VK_TRUE : VK_FALSE;
  auto fillStencil = [&](const StencilOpDesc& s) {
    VkStencilOpState o {};
    o.failOp = ToVkStencilOp(s.Fail);
    o.depthFailOp = ToVkStencilOp(s.DepthFail);
    o.passOp = ToVkStencilOp(s.Pass);
    o.compareOp = ToVkCompareOp(s.Compare);
    o.compareMask = desc.DepthStencil.StencilReadMask;
    o.writeMask = desc.DepthStencil.StencilWriteMask;
    return o;
  };
  depthStencil.front = fillStencil(desc.DepthStencil.StencilFront);
  depthStencil.back = fillStencil(desc.DepthStencil.StencilBack);

  VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo {};
  graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphicsPipelineCreateInfo.pNext = &renderingCreateInfo;
  graphicsPipelineCreateInfo.stageCount = stageCount;
  graphicsPipelineCreateInfo.pStages = stages;
  graphicsPipelineCreateInfo.pVertexInputState = &vertexInput;
  graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssembly;
  graphicsPipelineCreateInfo.pViewportState = &viewportState;
  graphicsPipelineCreateInfo.pRasterizationState = &rasterizer;
  graphicsPipelineCreateInfo.pMultisampleState = &multisample;
  graphicsPipelineCreateInfo.pDepthStencilState =
      (desc.DepthStencilFormat != DataFormat::None) ? &depthStencil : nullptr;
  graphicsPipelineCreateInfo.pColorBlendState = &colorBlend;
  graphicsPipelineCreateInfo.pDynamicState = &dynamicState;
  graphicsPipelineCreateInfo.layout = layout;

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkResult res =
      vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline);
  vkDestroyShaderModule(m_Device, vs, nullptr);
  if (ps != VK_NULL_HANDLE)
    vkDestroyShaderModule(m_Device, ps, nullptr);

  if (res != VK_SUCCESS)
  {
    vkDestroyPipelineLayout(m_Device, layout, nullptr);
    if (dsl != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(m_Device, dsl, nullptr);
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: vkCreateGraphicsPipelines failed (%d)", static_cast<i32>(res));
    return GraphicsPipeline {};
  }

  VulkanPipelineData* pd = AllocObject<VulkanPipelineData>();
  pd->Layout = layout;
  pd->Pipeline = pipeline;
  pd->DescSetLayout = dsl;
  pd->NumBindings = numBindings;
  pd->PushConstantBytes = desc.PushConstantBytes;
  pd->IsCompute = false;
  for (u32 i = 0; i < numBindings; ++i)
    pd->BindingTypes[i] = bindingTypes[i];

  if (desc.DebugName != nullptr)
    SetObjectName(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<u64>(pipeline), desc.DebugName);

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
    FreeObject(d);
  });
  return gp;
}

ComputePipeline VulkanDevice::CreateComputePipeline(const ComputePipelineDesc& desc) noexcept
{
  GECKO_PROFILE_NAMED(labels::Vulkan, "VulkanDevice::CreateComputePipeline");

  if (!desc.IsValid() || !m_Valid)
    return ComputePipeline {};

  VkShaderModule cs = CreateShaderModule(desc.ComputeShader);
  if (cs == VK_NULL_HANDLE)
    return ComputePipeline {};

  // -- Descriptor set layout -------------------------------------
  VkDescriptorSetLayoutBinding bindings[VulkanPipelineData::MaxBindings] {};
  VkDescriptorType bindingTypes[VulkanPipelineData::MaxBindings] {};
  u32 numBindings = 0;

  for (u32 r = 0; r < desc.NumPipelineResources; ++r)
  {
    const auto& resource = desc.PipelineResources[r];
    const VkDescriptorType dtype = ToVkDescriptorType(resource.Type, /*isCompute*/ true);
    if (dtype == VK_DESCRIPTOR_TYPE_MAX_ENUM)
      continue;
    for (u32 n = 0; n < resource.NumResources && numBindings < VulkanPipelineData::MaxBindings; ++n)
    {
      const u32 slot = numBindings;
      bindings[slot].binding = slot;
      bindings[slot].descriptorType = dtype;
      bindings[slot].descriptorCount = 1;
      bindings[slot].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      bindingTypes[slot] = dtype;
      ++numBindings;
    }
  }

  VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  if (numBindings > 0)
  {
    VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo {};
    descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descSetLayoutCreateInfo.bindingCount = numBindings;
    descSetLayoutCreateInfo.pBindings = bindings;
    VULKAN_CHECK(vkCreateDescriptorSetLayout(m_Device, &descSetLayoutCreateInfo, nullptr, &dsl));
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &dsl;
  }

  VkPushConstantRange pushConstantRange {};
  if (desc.PushConstantBytes > 0)
  {
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = desc.PushConstantBytes;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
  }

  VkPipelineLayout layout = VK_NULL_HANDLE;
  VULKAN_CHECK(vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &layout));

  VkPipelineShaderStageCreateInfo stage {};
  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage.module = cs;
  stage.pName = desc.ComputeShader.Entry;

  VkComputePipelineCreateInfo computePipelineCreateInfo {};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.stage = stage;
  computePipelineCreateInfo.layout = layout;

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkResult res = vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline);
  vkDestroyShaderModule(m_Device, cs, nullptr);

  if (res != VK_SUCCESS)
  {
    vkDestroyPipelineLayout(m_Device, layout, nullptr);
    if (dsl != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(m_Device, dsl, nullptr);
    GECKO_ERROR(labels::Vulkan, "VulkanDevice: vkCreateComputePipelines failed (%d)", static_cast<i32>(res));
    return ComputePipeline {};
  }

  VulkanPipelineData* pd = AllocObject<VulkanPipelineData>();
  pd->Layout = layout;
  pd->Pipeline = pipeline;
  pd->DescSetLayout = dsl;
  pd->NumBindings = numBindings;
  pd->PushConstantBytes = desc.PushConstantBytes;
  pd->IsCompute = true;
  for (u32 i = 0; i < numBindings; ++i)
    pd->BindingTypes[i] = bindingTypes[i];

  if (desc.DebugName != nullptr)
    SetObjectName(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<u64>(pipeline), desc.DebugName);

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
    FreeObject(d);
  });
  return cp;
}

// ------------------------------------------------------------
// Sampler
// ------------------------------------------------------------

Sampler VulkanDevice::CreateSampler(const SamplerDesc& desc) noexcept
{
  if (!m_Valid)
    return Sampler {};

  VkSamplerCreateInfo samplerCreateInfo {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = desc.Filter == SamplerFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  samplerCreateInfo.minFilter = samplerCreateInfo.magFilter;
  samplerCreateInfo.mipmapMode =
      desc.Filter == SamplerFilter::Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
  const VkSamplerAddressMode addressMode =
      desc.WrapMode == SamplerWrapMode::Wrap ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerCreateInfo.addressModeU = addressMode;
  samplerCreateInfo.addressModeV = addressMode;
  samplerCreateInfo.addressModeW = addressMode;
  samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;

  VkSampler vkSampler = VK_NULL_HANDLE;
  if (vkCreateSampler(m_Device, &samplerCreateInfo, nullptr, &vkSampler) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::CreateSampler failed");
    return Sampler {};
  }

  auto* sd = AllocObject<VulkanSamplerData>();
  sd->Sampler = vkSampler;

  if (desc.DebugName != nullptr)
    SetObjectName(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<u64>(vkSampler), desc.DebugName);

  Sampler s;
  s.Desc = desc;
  VulkanDevice* dev = this;
  s.Data = Shared<void>(sd, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanSamplerData*>(p);
    if (d->Sampler != VK_NULL_HANDLE)
      vkDestroySampler(dev->m_Device, d->Sampler, nullptr);
    FreeObject(d);
  });
  return s;
}

// ------------------------------------------------------------
// Timestamp query pool
// ------------------------------------------------------------

QueryPool VulkanDevice::CreateTimestampQueryPool(const QueryPoolDesc& desc) noexcept
{
  if (!m_Valid || !desc.IsValid())
    return QueryPool {};

  VkQueryPoolCreateInfo queryPoolCreateInfo {};
  queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
  queryPoolCreateInfo.queryCount = desc.Count;

  VkQueryPool vkPool = VK_NULL_HANDLE;
  if (vkCreateQueryPool(m_Device, &queryPoolCreateInfo, nullptr, &vkPool) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::CreateTimestampQueryPool failed");
    return QueryPool {};
  }

  // Reset the whole pool up front so WriteTimestamp is legal before the
  // first Reset call on a command list. Prefer host reset when supported;
  // otherwise issue a one-time command-buffer reset.
  if (m_HasHostQueryReset)
  {
    vkResetQueryPool(m_Device, vkPool, 0, desc.Count);
  }
  else
  {
    struct ResetCtx
    {
      VkQueryPool Pool;
      u32 Count;
    } rc {vkPool, desc.Count};
    OneTimeSubmit(
        [](VkCommandBuffer cb, void* ctx) {
          auto* r = static_cast<ResetCtx*>(ctx);
          vkCmdResetQueryPool(cb, r->Pool, 0, r->Count);
        },
        &rc);
  }

  auto* qd = AllocObject<VulkanQueryPoolData>();
  qd->QueryPool = vkPool;
  qd->Count = desc.Count;
  qd->TimestampPeriodNs = m_TimestampPeriodNs;

  if (desc.DebugName != nullptr)
    SetObjectName(VK_OBJECT_TYPE_QUERY_POOL, reinterpret_cast<u64>(vkPool), desc.DebugName);

  QueryPool q;
  q.Desc = desc;
  VulkanDevice* dev = this;
  q.Data = Shared<void>(qd, [dev](void* p) noexcept {
    auto* d = static_cast<VulkanQueryPoolData*>(p);
    if (d->QueryPool != VK_NULL_HANDLE)
      vkDestroyQueryPool(dev->m_Device, d->QueryPool, nullptr);
    FreeObject(d);
  });
  return q;
}

u32 VulkanDevice::ReadTimestamps(const QueryPool& pool, u32 firstQuery, ::std::span<u64> out) noexcept
{
  if (!pool.IsValid() || out.empty())
    return 0;
  auto* qd = static_cast<VulkanQueryPoolData*>(pool.Data.get());
  if (firstQuery >= qd->Count)
    return 0;
  const u32 count = static_cast<u32>(::std::min<::std::size_t>(out.size(), qd->Count - firstQuery));

  ::std::size_t stagingCount = count;
  u64 staging[64];
  ::std::vector<u64> heapStaging;
  u64* ticks = staging;
  if (stagingCount > 64)
  {
    heapStaging.resize(stagingCount);
    ticks = heapStaging.data();
  }

  VkResult res = vkGetQueryPoolResults(m_Device, qd->QueryPool, firstQuery, count, sizeof(u64) * count, ticks,
                                       sizeof(u64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  if (res != VK_SUCCESS)
    return 0;

  const f32 periodNs = qd->TimestampPeriodNs;
  for (u32 i = 0; i < count; ++i)
    out[i] = static_cast<u64>(static_cast<f64>(ticks[i]) * static_cast<f64>(periodNs));
  return count;
}

// ------------------------------------------------------------
// GPU profiler
// ------------------------------------------------------------

void VulkanDevice::HostResetQueryPool(const QueryPool& pool, u32 firstQuery, u32 count) noexcept
{
  if (!pool.IsValid() || count == 0)
    return;
  auto* qd = static_cast<VulkanQueryPoolData*>(pool.Data.get());
  if (firstQuery >= qd->Count)
    return;
  const u32 c = ::std::min(count, qd->Count - firstQuery);
  if (m_HasHostQueryReset)
  {
    vkResetQueryPool(m_Device, qd->QueryPool, firstQuery, c);
  }
  else
  {
    struct Ctx
    {
      VkQueryPool Pool;
      u32 First;
      u32 Count;
    } cc {qd->QueryPool, firstQuery, c};
    OneTimeSubmit(
        [](VkCommandBuffer cb, void* x) {
          auto* r = static_cast<Ctx*>(x);
          vkCmdResetQueryPool(cb, r->Pool, r->First, r->Count);
        },
        &cc);
  }
}

::gecko::Unique<IGpuSampler> VulkanDevice::CreateGpuSampler(const GpuSamplerDesc& desc) noexcept
{
  if (!m_Valid)
    return nullptr;
  auto sampler = ::gecko::CreateUnique<VulkanGpuSampler>(*this, desc);
  if (sampler == nullptr || !sampler->IsValid())
    return nullptr;
  return sampler;
}

// ------------------------------------------------------------
// Debug naming
// ------------------------------------------------------------

void VulkanDevice::SetObjectName(VkObjectType type, u64 handle, const char* name) const noexcept
{
  if (!m_HasDebugUtils || name == nullptr || handle == 0)
    return;
  auto fn = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
      vkGetInstanceProcAddr(m_Instance, "vkSetDebugUtilsObjectNameEXT"));
  if (fn == nullptr)
    return;
  VkDebugUtilsObjectNameInfoEXT info {};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  info.objectType = type;
  info.objectHandle = handle;
  info.pObjectName = name;
  fn(m_Device, &info);
}

// ------------------------------------------------------------
// Uploads
// ------------------------------------------------------------

void VulkanDevice::OneTimeSubmit(void (*record)(VkCommandBuffer, void*), void* ctx) noexcept
{
  // Use the caller's per-thread pool so this can be invoked from any
  // thread (texture/buffer uploads may happen off the main thread).
  VkCommandPool pool = AcquireThreadCommandPool();

  VkCommandBufferAllocateInfo cmdBufAllocInfo {};
  cmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmdBufAllocInfo.commandPool = pool;
  cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdBufAllocInfo.commandBufferCount = 1;

  VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
  vkAllocateCommandBuffers(m_Device, &cmdBufAllocInfo, &cmdBuf);

  VkCommandBufferBeginInfo beginInfo {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);
  record(cmdBuf, ctx);
  vkEndCommandBuffer(cmdBuf);

  VkSubmitInfo submitInfo {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuf;
  {
    ::std::lock_guard<::std::mutex> lock(m_QueueMutex);
    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_GraphicsQueue);
  }
  vkFreeCommandBuffers(m_Device, pool, 1, &cmdBuf);
}

void VulkanDevice::UploadTextureData(Texture& texture, ::std::span<const ::gecko::byte> data, u32 mip,
                                     u32 slice) noexcept
{
  if (!texture.IsValid() || data.empty())
    return;
  auto* td = static_cast<VulkanTextureData*>(texture.Data.get());

  // Staging buffer with mapped CPU memory.
  VkBufferCreateInfo bufferCreateInfo {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = data.size();
  bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
  allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBuffer staging = VK_NULL_HANDLE;
  VmaAllocation stagingAlloc = nullptr;
  VmaAllocationInfo info {};
  if (vmaCreateBuffer(m_Allocator, &bufferCreateInfo, &allocCreateInfo, &staging, &stagingAlloc, &info) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::UploadTextureData staging allocation failed");
    return;
  }
  ::std::memcpy(info.pMappedData, data.data(), data.size());

  struct Ctx
  {
    VulkanTextureData* TD;
    VkBuffer Src;
    u32 Mip;
    u32 Slice;
    VkImageLayout OldLayout;
  } ctx {td, staging, mip, slice, td->CurrentLayout};

  OneTimeSubmit(
      [](VkCommandBuffer cmdBuf, void* c) {
        auto* x = static_cast<Ctx*>(c);

        auto transition = [&](VkImageLayout oldL, VkImageLayout newL, VkAccessFlags srcA, VkAccessFlags dstA,
                              VkPipelineStageFlags srcS, VkPipelineStageFlags dstS) {
          VkImageMemoryBarrier barrier {};
          barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
          barrier.oldLayout = oldL;
          barrier.newLayout = newL;
          barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          barrier.image = x->TD->Image;
          barrier.subresourceRange.aspectMask = x->TD->Aspect;
          barrier.subresourceRange.baseMipLevel = x->Mip;
          barrier.subresourceRange.levelCount = 1;
          barrier.subresourceRange.baseArrayLayer = x->Slice;
          barrier.subresourceRange.layerCount = 1;
          barrier.srcAccessMask = srcA;
          barrier.dstAccessMask = dstA;
          vkCmdPipelineBarrier(cmdBuf, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        };

        transition(x->OldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        const u32 mipW = (x->TD->Width >> x->Mip) > 0 ? (x->TD->Width >> x->Mip) : 1;
        const u32 mipH = (x->TD->Height >> x->Mip) > 0 ? (x->TD->Height >> x->Mip) : 1;

        VkBufferImageCopy copyRegion {};
        copyRegion.imageSubresource.aspectMask = x->TD->Aspect;
        copyRegion.imageSubresource.mipLevel = x->Mip;
        copyRegion.imageSubresource.baseArrayLayer = x->Slice;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = {mipW, mipH, 1};
        vkCmdCopyBufferToImage(cmdBuf, x->Src, x->TD->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
      },
      &ctx);

  td->CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vmaDestroyBuffer(m_Allocator, staging, stagingAlloc);
}

void VulkanDevice::UploadBufferData(Buffer& buffer, ::std::span<const ::gecko::byte> data, u32 offset) noexcept
{
  if (!buffer.IsValid() || data.empty())
    return;
  auto* bufferData = static_cast<VulkanBufferData*>(buffer.Data.get());

  // Create a staging buffer
  VkBufferCreateInfo bufferCreateInfo {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = data.size();
  bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
  allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBuffer staging = VK_NULL_HANDLE;
  VmaAllocation stagingAlloc = nullptr;
  VmaAllocationInfo info {};
  if (vmaCreateBuffer(m_Allocator, &bufferCreateInfo, &allocCreateInfo, &staging, &stagingAlloc, &info) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanDevice::UploadBufferData staging allocation failed");
    return;
  }

  ::std::memcpy(info.pMappedData, data.data(), data.size());

  struct Ctx
  {
    VkBuffer src;
    VkBuffer dst;
    VkDeviceSize size;
    u32 dstOffset;
  } ctx {staging, bufferData->Buffer, data.size(), offset};

  OneTimeSubmit(
      [](VkCommandBuffer cmdBuf, void* c) {
        auto* x = static_cast<Ctx*>(c);
        VkBufferCopy copyRegion {};
        copyRegion.dstOffset = x->dstOffset;
        copyRegion.size = x->size;
        vkCmdCopyBuffer(cmdBuf, x->src, x->dst, 1, &copyRegion);
      },
      &ctx);

  vmaDestroyBuffer(m_Allocator, staging, stagingAlloc);
}

}  // namespace gecko::graphics
#endif
