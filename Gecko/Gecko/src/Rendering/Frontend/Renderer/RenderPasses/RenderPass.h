#pragma once

#ifdef WIN32
#pragma warning( push )
#pragma  warning(disable : 4100)
#endif

#include "Defines.h"

#include "Rendering/Backend/Objects.h"
#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

namespace Gecko
{

class ResourceManager;
struct SceneDescriptor;
class CommandList;

class RenderPass
{
public:
	RenderPass() = default;
	virtual ~RenderPass() {}

	virtual const void Init(ResourceManager* resourceManager) = 0;
	virtual const void Render(const SceneDescriptor& sceneDescriptor ,ResourceManager* resourceManager, Ref<CommandList> commandList) = 0;
protected:

private:
	
};

}