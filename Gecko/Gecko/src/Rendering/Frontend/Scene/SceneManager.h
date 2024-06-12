#pragma once

#include "Defines.h"

namespace Gecko
{

class Scene;
class ResourceManager;

class SceneManager
{
public:

	SceneManager() = default;
	~SceneManager() {};

	void Init();
	void Shutdown();

	Ref<Scene> CreateScene();
	Ref<Scene> LoadGLTFScene(const std::string& pathString, ResourceManager* resourceManager);


	// TODO: Resizeing of each scene to update the cameras
	static void Resize() {};

	void ImGuiRender();

private:
	std::vector<Ref<Scene>> m_Scenes;
};

}