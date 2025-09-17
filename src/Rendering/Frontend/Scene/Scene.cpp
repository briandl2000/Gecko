#include "Rendering/Frontend/Scene/Scene.h"

#include "Rendering/Frontend/Scene/SceneRenderInfo.h"

namespace Gecko  {

  void Scene::Init(const std::string& name)
  {
    AddEventListener(Event::SystemEvent::CODE_RESIZED, &Scene::OnResize);

    m_Name = name;
  }

  SceneRenderInfo Scene::GetSceneRenderInfo() const
  {
    SceneRenderInfo sceneRenderInfo;

    PopulateSceneRenderInfo(&sceneRenderInfo);

    return sceneRenderInfo;
  }


  const std::string& Scene::GetName() const
  {
    return m_Name;
  }

#pragma warning(push)
#pragma warning(disable: 4100)
  void Scene::PopulateSceneRenderInfo(SceneRenderInfo* sceneRenderInfo) const
  {
    
  }
#pragma warning(pop)
}
