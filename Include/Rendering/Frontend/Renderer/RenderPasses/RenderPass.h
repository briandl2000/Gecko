#pragma once

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4100)
#endif

#include <vector>
#include <string>

#include "Defines.h"

#include "Rendering/Backend/Objects.h"
#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Scene/SceneRenderInfo.h"

#include "Core/Platform.h"

namespace Gecko
{

class ResourceManager;
class Renderer;
class CommandList;
using RenderPassHandle = std::string;

class RenderPassInterface
{
public:
	struct ConfigDataInterface
	{};

	virtual const void Init(const Platform::AppInfo& appInfo, ResourceManager* resourceManager,
		const ConfigDataInterface& dependencies = ConfigDataInterface()) = 0;
	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) = 0;

	virtual RenderTargetHandle GetOutputHandle() const = 0;

};

// Shortened to make inheriting from this type easier
using BaseConfigData = RenderPassInterface::ConfigDataInterface;

template <typename T>
class RenderPass : public RenderPassInterface
{
public:
	RenderPass() = default;
	virtual ~RenderPass() {}

	virtual const void Init(const Platform::AppInfo& appInfo, ResourceManager* resourceManager,
		const BaseConfigData& dependencies) override final
	{
		const T::ConfigData& data = static_cast<const T::ConfigData&>(dependencies);
		static_cast<T*>(this)->SubInit(appInfo, resourceManager, data);
	}

	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) override = 0;

	virtual RenderTargetHandle GetOutputHandle() const override final
	{
		return m_OutputHandle;
	}

protected:
	RenderTargetHandle m_OutputHandle;

private:
	
};

}