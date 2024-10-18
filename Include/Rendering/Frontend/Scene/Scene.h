#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Scene/SceneObjects/SceneCamera.h"
#include "Rendering/Frontend/Scene/SceneObjects/SceneLight.h"
#include "Rendering/Frontend/Scene/SceneObjects/SceneRenderObject.h"

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

	// TODO: add names to scenes and scene nodes

	class SceneNode
	{
	public:
		friend class Scene;

		SceneNode() = default;
		~SceneNode() {};

		[[nodiscard]] SceneNode* AddNode(const std::string& name);
	
		void AppendSceneRenderObject(SceneRenderObject* sceneRenderObject);
		void AppendLight(SceneLight* light);
		void AttachCamera(SceneCamera* camera);
		void AppendScene(Scene* scene);
		
		u32 GetChildrenCount();
		SceneNode* GetChild(u32 nodeIndex);

		const void SetName(const std::string& name);
		const std::string& GetName();

	public:
		NodeTransform Transform;

	private:
		const void PopulateSceneRenderInfo(SceneRenderInfo& sceneRenderInfo, glm::mat4 transform) const;

	private:
		std::vector<Scope<SceneNode>> m_Children;

		std::vector<Scene*> m_Scenes;
		std::vector<SceneRenderObject*> m_SceneRenderObjects;
		std::vector<SceneLight*> m_Lights;
		SceneCamera* m_Camera{ nullptr };

		std::string m_Name{ "Node" };
	};

	class Scene
	{
	public:
		friend class SceneNode;
		friend class SceneManager;

		Scene() = default;
		~Scene() {};

		void Init(const std::string& name);
	
		SceneNode* GetRootNode();

		[[nodiscard]] SceneRenderObject* CreateSceneRenderObject();
		[[nodiscard]] SceneCamera* CreateCamera();
		[[nodiscard]] SceneLight* CreateLight(LightType lightType);
		[[nodiscard]] SceneDirectionalLight* CreateDirectionalLight();
		[[nodiscard]] ScenePointLight* CreatePointLight();
		[[nodiscard]] SceneSpotLight* CreateSpotLight();
		
		[[nodiscard]] const SceneRenderInfo GetSceneRenderInfo() const;

		EnvironmentMapHandle GetEnvironmentMapHandle() const;
		void SetEnvironmentMapHandle(EnvironmentMapHandle handle);

		const std::string& GetName() { return m_Name; };

	private:
		void OnResize(u32 width, u32 height);

		const void PopulateSceneRenderInfo(SceneRenderInfo& sceneRenderInfo, glm::mat4 transform) const;
	
	private:
		Scope<SceneNode> m_RootNode;

		std::vector<Scope<SceneRenderObject>> m_SceneRenderObject;
		std::vector<Scope<SceneLight>> m_Lights;
		std::vector<Scope<SceneCamera>> m_Cameras;

		EnvironmentMapHandle m_EnvironmentMapHandle{ 0 };

		std::string m_Name{ "Scene" };

	};

}