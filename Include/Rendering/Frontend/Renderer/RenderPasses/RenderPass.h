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
	// Override this in derived classes (even if you don't need any config data)
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

// Inherit from this class using : public RenderPass<DerivedClassName> (or : public Gecko::RenderPass<DerivedClassName>)
template <typename T>
class RenderPass : public RenderPassInterface
{
public:
	RenderPass() = default;
	virtual ~RenderPass() {}

	/* This function will cause a runtime error if the object it is called on is not a valid derived class of RenderPass,
	   if no overridden ConfigData struct exists on the derived class, or if no SubInit function exists on the derived class */
	virtual const void Init(const Platform::AppInfo& appInfo, ResourceManager* resourceManager,
		const BaseConfigData& dependencies) override final
	{
		const T::ConfigData& data = static_cast<const T::ConfigData&>(dependencies);
		if (T* t = dynamic_cast<T*>(this))
			t->SubInit(appInfo, resourceManager, data);
		else
			ASSERT_MSG(false, "Invalid render pass initialisation!");
	}

	/* Render to output target (pointed to by m_OutputHandle). Any dependencies for rendering should be defined in derived 
	classes; these are usually set up by defining a ConfigData struct that inherits from BaseConfigData, and passing a 
	valid ConfigData into the Init() function */
	virtual const void Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
		const Renderer* renderer, Ref<CommandList> commandList) override = 0;

	virtual RenderTargetHandle GetOutputHandle() const override final
	{
		return m_OutputHandle;
	}

protected:
	// Make sure to write a SubInit function!

	RenderTargetHandle m_OutputHandle{ static_cast<u32>(-1) };

private:
	
};

}