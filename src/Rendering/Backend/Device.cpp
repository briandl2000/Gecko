#include "Rendering/Backend/Device.h"

#ifdef DIRECTX_12
#include "Rendering/DX12/Device_DX12.h"
#endif // DIRECTX_12

#include "Core/Asserts.h"

namespace Gecko {

  Ref<Device> Device::CreateDevice()
  {
    switch (s_RenderAPI)
    {
    case RenderAPI::None:
      ASSERT(false, "No RenderAPI is selected");
      break;
#ifdef DIRECTX_12
    case RenderAPI::DX12:
      return CreateRef<DX12::Device_DX12>();
      break;
#endif // DIRECTX_12
    }

    ASSERT(false, "Unknown RenderAPI selected");
    return nullptr;
  }
}
