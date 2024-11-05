#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Scene/SceneObjects/SceneCamera.h"
#include "Rendering/Frontend/Scene/SceneObjects/SceneLight.h"
#include "Rendering/Frontend/Scene/SceneObjects/SceneRenderObject.h"
#include "Core/Event.h"

namespace Gecko {

	struct SceneRenderInfo;
	class ResourceManager;

	class NodeTransform
	{
	public:

		NodeTransform() = default;

		glm::vec3 Position{ 0.f };
		glm::vec3 Rotation{ 0.f };
		glm::vec3 Scale{ 1.f };

		inline glm::mat4 GetMat4() const
		{
			glm::mat4 nodeTranslationMatrix = glm::translate(glm::mat4(1.), Position);
			glm::mat4 nodeRotationMatrix = glm::toMat4(glm::quat(glm::radians(Rotation)));
			glm::mat4 nodeScaleMatrix = glm::scale(glm::mat4(1.), Scale);

			return nodeTranslationMatrix * nodeRotationMatrix * nodeScaleMatrix;
		}
	};

	class Scene;

	class SceneNode
	{
	public:
		friend class Scene;

		SceneNode() = default;
		~SceneNode() {};

		[[nodiscard]] SceneNode* AddNode(const std::string& name);
	
		// Transfers ownership of sceneRenderObject to this node, sets passed Scope to nullptr
		void AppendSceneRenderObject(Scope<SceneRenderObject>* sceneRenderObject);
		// Transfers ownership of light to this node, sets passed Scope to nullptr
		void AppendLight(Scope<SceneLight>* light);
		// Transfers ownership of camera to this node, sets passed Scope to nullptr
		void AttachCamera(Scope<SceneCamera>* camera);

		// Copy data from scene into a new child node, leaves passed scene intact
		void AppendSceneData(const Scene& scene);
		
		u32 GetChildrenCount() const;
		SceneNode* GetChild(u32 nodeIndex) const;

		const void SetName(const std::string& name);
		const std::string& GetName() const;

		// Adjust aspect ratios of cameras
		bool SceneNode::OnResize(const Event::EventData& data);

	public:
		NodeTransform Transform;

	private:
		// Recursively copy the data from this node onto target
		void RecursiveCopy(SceneNode* target) const;

		const void PopulateSceneRenderInfo(SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const;

	private:
		std::vector<Scope<SceneNode>> m_Children;

		std::vector<Scope<SceneRenderObject>> m_SceneRenderObjects;
		std::vector<Scope<SceneLight>> m_Lights;
		Scope<SceneCamera> m_Camera{ nullptr };

		std::string m_Name{ "Node" };
	};

	class Scene : Event::EventListener<Scene>
	{
	public:
		friend class SceneNode;
		friend class SceneManager;

		Scene() = default;
		~Scene() {};

		void Init(const std::string& name);
	
		SceneNode* GetRootNode() const;

		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] SceneRenderObject* CreateSceneRenderObject() const;
		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] SceneCamera* CreateCamera() const;
		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] SceneLight* CreateLight(LightType lightType) const;
		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] SceneDirectionalLight* CreateDirectionalLight() const;
		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] ScenePointLight* CreatePointLight() const;
		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] SceneSpotLight* CreateSpotLight() const;
		
		[[nodiscard]] const SceneRenderInfo GetSceneRenderInfo() const;

		EnvironmentMapHandle GetEnvironmentMapHandle() const;
		void SetEnvironmentMapHandle(EnvironmentMapHandle handle);

		const std::string& GetName() const;

		// Adjust aspect ratios of cameras
		bool Scene::OnResize(const Event::EventData& data);

	private:
		const void PopulateSceneRenderInfo(SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const;

		// Copies the data in this scene to a SceneNode, so it can be appended to another scene
		[[nodiscard]] Scope<SceneNode> CopySceneToNode() const;
	
	private:
		Scope<SceneNode> m_RootNode;

		EnvironmentMapHandle m_EnvironmentMapHandle{ 0 };

		std::string m_Name{ "Scene" };

	};

}