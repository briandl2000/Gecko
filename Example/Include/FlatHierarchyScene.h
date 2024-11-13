#pragma once

#include "Rendering/Frontend/Scene/Scene.h"
#include <vector>

class FlatHierarchyScene : public Gecko::Scene
{
public:
	FlatHierarchyScene() = default;
	virtual ~FlatHierarchyScene() {}

	virtual void Init(const std::string& name) override;

	// Adjust aspect ratios of cameras
	virtual bool FlatHierarchyScene::OnResize(const Gecko::Event::EventData& data) override;

	// Transfers ownership of object to this scene, sets passed Scope to nullptr
	void AppendSceneRenderObject(Gecko::Scope<Gecko::SceneRenderObject>* object);
	// Transfers ownership of light to this scene, sets passed Scope to nullptr
	void AppendLight(Gecko::Scope<Gecko::SceneLight>* light);
	// Transfers ownership of camera to this scene, sets passed Scope to nullptr
	void AttachMainCamera(Gecko::Scope<Gecko::SceneCamera>* camera);
	// Transfers ownership of camera to this scene, sets passed Scope to nullptr
	void AppendAdditionalCamera(Gecko::Scope<Gecko::SceneCamera>* camera);
	// Transfers onwership of all objects in scene to this scene, clears containers of passed scene
	void AppendSceneData(FlatHierarchyScene* otherScene);

	inline const std::vector<Gecko::Scope<Gecko::SceneRenderObject>>& GetSceneObjects() const { return m_SceneRenderObjects; }
	inline const std::vector<Gecko::Scope<Gecko::SceneLight>>& GetSceneLights() const { return m_Lights; }
	inline const std::vector<Gecko::Scope<Gecko::SceneCamera>>& GetSceneCameras() const { return m_AdditionalCameras; }
	inline const Gecko::Scope<Gecko::SceneCamera>& GetMainCamera() const { return m_MainCamera; }

protected:
	virtual const void PopulateSceneRenderInfo(Gecko::SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const;

private:
	std::vector<Gecko::Scope<Gecko::SceneRenderObject>> m_SceneRenderObjects{};
	std::vector<Gecko::Scope<Gecko::SceneLight>> m_Lights{};
	std::vector<Gecko::Scope<Gecko::SceneCamera>> m_AdditionalCameras{};
	Gecko::Scope<Gecko::SceneCamera> m_MainCamera{ nullptr };
};
