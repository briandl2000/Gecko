#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Scene/SceneObjects.h"


namespace Gecko
{

class ResourceManager;

class Scene;

class SceneNode
{
public:
	friend class Scene;

	SceneNode() = default;
	~SceneNode() {};

	Ref<SceneNode> AddNode();

	void AddMeshInstance(MeshHandle meshHandle, MaterialHandle materialHandle);
	void AddLight();
	void AddCamera(f32 FOV, f32 near = 0.1f, f32 far = 100.f, f32 aspectRatio = 1.f, bool keepAspect = true);
	void AddScene(Ref<Scene> scene);

public:
	NodeTransform Transform;

private:

	void ImGuiRender();

	u32 m_ParentIndex{ 0 };
	u32 m_Index{ 0 };

	std::vector<Ref<Scene>> m_Scenes;
	std::vector<MeshInstance> m_MeshesInstances;
	std::vector<Light> m_Lights;
	Camera camera;
	bool hasCamera = false;

	Scene* m_Scene;
};

class Scene
{
public:
	friend class SceneNode;

	Scene() = default;
	~Scene() {};

	void Init();
	
	const SceneDescriptor GetSceneDescriptor(glm::mat4 transform = glm::mat4(1.f)) const;
	Ref<SceneNode> GetRootNode();

	EnvironmentMapHandle EnvironmentMapHandle{ 0 };

	void ImGuiRender();

private:
	void DisplayNode(u32 nodeIndex, u32& selectedNode);

	const void PopulateSceneDescriptor(SceneDescriptor& sceneDescriptor, glm::mat4 transform) const;
	Ref<SceneNode> CreateNode(u32 parentIndex = 0);
	u32 m_RootNodeIndex{ 0 };
	std::vector<Ref<SceneNode>> m_Nodes;

};

}