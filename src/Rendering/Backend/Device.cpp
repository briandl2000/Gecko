#include "Rendering/Backend/Device.h"

#include "Rendering/Backend/CommandList.h"

#ifdef WIN32
#include "Rendering/DX12/Device_DX12.h"
#endif

#include "Core/Asserts.h"

namespace Gecko {

	Ref<Device> Device::CreateDevice()
	{
		switch (s_RenderAPI)
		{
		case RenderAPI::None:
			ASSERT(false, "No RenderAPI is selected");
			break;
#ifdef WIN32
		case RenderAPI::DX12:
			return CreateRef<DX12::Device_DX12>();
			break;
#endif // WIN32
		}

		ASSERT(false, "Unkown RenderAPI selected");
		return nullptr;
	}
}