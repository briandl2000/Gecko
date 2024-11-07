#pragma once

#include "Defines.h"

#include "Rendering/Frontend/ApplicationContext.h"

namespace Gecko {

	class DebugUIRenderer
	{
	public:
		static bool RenderDebugUI(ApplicationContext& ctx);
	};

}