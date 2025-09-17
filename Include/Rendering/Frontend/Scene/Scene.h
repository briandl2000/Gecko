#pragma once

#include "Defines.h"

#include "Core/Event.h"

namespace Gecko {

  struct SceneRenderInfo;

  class Scene : Event::EventListener<Scene>
  {
  public:
    Scene() = default;
    virtual ~Scene() {};

    // Sets the name of this scene and initialises event listening. Should be called right after scene creation
    virtual void Init(const std::string& name);
    
    // Returns a newly created SceneRenderInfo object filled with data from this scene
    [[nodiscard]] SceneRenderInfo GetSceneRenderInfo() const;

    // Adjust aspect ratios of cameras
    virtual bool OnResize(const Event::EventData& data) = 0;

  public:
    const std::string& GetName() const;

  protected:
    // Read data from this scene into a SceneRenderInfo object (how scene data is translated into the object should be defined
    // by derived classes based on their internal structure).
    // Don't forget to call this parent function if you want the environment map handle to be put in.
    virtual void PopulateSceneRenderInfo(SceneRenderInfo* sceneRenderInfo) const = 0;
  
  private:
    std::string m_Name{ "Scene" };
  };

}
