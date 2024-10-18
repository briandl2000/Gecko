#pragma once

#ifdef WIN32
#pragma warning( push )
#pragma  warning(disable : 4100)
#endif

#include "Defines.h"

#include "Rendering/Backend/Objects.h"
#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Scene/SceneRenderInfo.h"

#include "Platform/Platform.h"

namespace Gecko
{

class ResourceManager;
class CommandList;

class RenderPass
{
public:
	RenderPass() = default;
	virtual ~RenderPass() {}

	virtual const void Init(Platform::AppInfo& appInfo, ResourceManager* resourceManager) = 0;
	virtual const void Render(const SceneRenderInfo& sceneRenderInfo,ResourceManager* resourceManager, Ref<CommandList> commandList) = 0;

protected:

private:
	
};

}