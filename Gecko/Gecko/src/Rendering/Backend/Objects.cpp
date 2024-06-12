#include "Rendering/Backend/Objects.h"

#include <cmath>

namespace Gecko {

	u32 CalculateNumberOfMips(u32 width, u32 height)
	{
		return 1+static_cast<u32>(std::log2(static_cast<float>(std::max(width, height))));
	}

	u32 FormatSizeInBytes(const Format& format)
	{
		switch (format)
		{
		case Format::R8G8B8A8_SRGB: return 4 * 1;
		case Format::R8G8B8A8_UNORM: return 4 * 1;
		case Format::R32G32_FLOAT: return 4 * 2;
		case Format::R32G32B32_FLOAT: return 4 * 3;
		case Format::R32G32B32A32_FLOAT: return 4 * 4;
		case Format::R16G16B16A16_FLOAT: return 4 * 2;

		case Format::R32_FLOAT: return 4;

		case Format::R8_UINT: return 1;
		case Format::R16_UINT: return 2;
		case Format::R32_UINT: return 4;

		case Format::R8_INT: return 1;
		case Format::R16_INT: return 2;
		case Format::R32_INT: return 4;

		case Format::None: break;
		}

		ASSERT_MSG(false, "Unkown Format");
		return 0;
	}

	u32 GetRenderTargetResourceIndex(const RenderTargetType& renderTargetType)
	{
		switch (renderTargetType)
		{
		case RenderTargetType::Target0:
			return 0;
			break;
		case RenderTargetType::Target1:
			return 1;
			break;
		case RenderTargetType::Target2:
			return 2;
			break;
		case RenderTargetType::Target3:
			return 3;
			break;
		case RenderTargetType::Target4:
			return 4;
			break;
		case RenderTargetType::Target5:
			return 5;
			break;
		case RenderTargetType::Target6:
			return 6;
			break;
		case RenderTargetType::Target7:
			return 7;
			break;
		case RenderTargetType::Target8:
			return 8;
			break;
		default:
			break;
		}

		ASSERT_MSG(false, "To get the resource index you have to pass a correct render target type from 0 to 8");
		return 0;
	}

}