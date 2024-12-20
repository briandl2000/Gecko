#pragma once

#include "Rendering/Frontend/Scene/Scene.h"
#include "Rendering/Frontend/Scene/SceneObjects/Transform.h"

class SceneNode;
class NodeBasedScene : public Gecko::Scene
{
public:
	friend class SceneNode;

	NodeBasedScene() = default;
	virtual ~NodeBasedScene() {}

	virtual void Init(const std::string& name) override;

	// Get a pointer to this scene's root node (modifiable)
	SceneNode* GetRootNode() const;

	// Adjust aspect ratios of cameras
	virtual bool OnResize(const Gecko::Event::EventData& data) override;

protected:
	virtual const void PopulateSceneRenderInfo(Gecko::SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const;

	// Copies the data in this scene to a SceneNode, so it can be appended to another scene
	[[nodiscard]] Gecko::Scope<SceneNode> CopySceneToNode() const;

private:
	Gecko::Scope<SceneNode> m_RootNode;
};

class SceneNode
{
public:
	friend class NodeBasedScene;

	SceneNode() = default;
	~SceneNode() {}

	[[nodiscard]] SceneNode* AddNode(const std::string& name);

	// Transfers ownership of sceneRenderObject to this node, sets passed Scope to nullptr
	void AppendSceneRenderObject(Gecko::Scope<Gecko::SceneRenderObject>* sceneRenderObject);
	// Transfers ownership of light to this node, sets passed Scope to nullptr
	void AppendLight(Gecko::Scope<Gecko::SceneLight>* light);
	// Transfers ownership of camera to this node, sets passed Scope to nullptr
	void AttachCamera(Gecko::Scope<Gecko::SceneCamera>* camera);

	// Copy data from scene into a new child node, leaves passed scene intact
	void AppendSceneData(const NodeBasedScene& scene);

	const Gecko::Transform& GetTransform() const;
	Gecko::Transform& GetModifiableTransform();

	Gecko::u32 GetChildrenCount() const;
	SceneNode* GetChild(Gecko::u32 nodeIndex) const;

	const void SetName(const std::string& name);
	const std::string& GetName() const;

	// Adjust aspect ratios of cameras
	bool OnResize(const Gecko::Event::EventData& data);

private:
	// Recursively copy the data from this node onto target
	void RecursiveCopy(SceneNode* target) const;

	const void PopulateSceneRenderInfo(Gecko::SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const;

private:
	Gecko::Transform m_Transform;
	
	std::vector<Gecko::Scope<SceneNode>> m_Children;

	std::vector<Gecko::Scope<Gecko::SceneRenderObject>> m_SceneRenderObjects;
	std::vector<Gecko::Scope<Gecko::SceneLight>> m_Lights;
	Gecko::Scope<Gecko::SceneCamera> m_Camera{ nullptr };

	std::string m_Name{ "Node" };
};
