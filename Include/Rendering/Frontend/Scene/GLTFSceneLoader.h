#pragma once

#include "Defines.h"

namespace Gecko
{

	class Scene;
	class ApplicationContext;

	class GLTFSceneLoader
	{
	public:
		[[nodiscard]] static Scene* LoadScene(const std::string& pathString, ApplicationContext& ctx);
	};

}