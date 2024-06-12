#pragma once

#include "Defines.h"

namespace Gecko
{

class Scene;
class ResourceManager;

class GLTFSceneLoader
{
public:
	static void LoadScene(Ref<Scene>& scene, const std::string& pathString, ResourceManager* resourceManager);
};


}