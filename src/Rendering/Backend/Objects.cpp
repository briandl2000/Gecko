#include "Rendering/Backend/Objects.h"

#include <cmath>

namespace Gecko
{

	u32 CalculateNumberOfMips(u32 width, u32 height)
	{
		return 1 + static_cast<u32>(std::log2(static_cast<float>(std::max(width, height))));
	}

	u32 FormatSizeInBytes(const DataFormat& format)
	{
		switch (format)
		{
		case DataFormat::R8G8B8A8_SRGB: return 4 * 1;
		case DataFormat::R8G8B8A8_UNORM: return 4 * 1;
		case DataFormat::R32G32_FLOAT: return 4 * 2;
		case DataFormat::R32G32B32_FLOAT: return 4 * 3;
		case DataFormat::R32G32B32A32_FLOAT: return 4 * 4;
		case DataFormat::R16G16B16A16_FLOAT: return 4 * 2;

		case DataFormat::R32_FLOAT: return 4;

		case DataFormat::R8_UINT: return 1;
		case DataFormat::R16_UINT: return 2;
		case DataFormat::R32_UINT: return 4;

		case DataFormat::R8_INT: return 1;
		case DataFormat::R16_INT: return 2;
		case DataFormat::R32_INT: return 4;

		case DataFormat::None: break;
		}

		ASSERT_MSG(false, "Unkown Format");
		return 0;
	}

}