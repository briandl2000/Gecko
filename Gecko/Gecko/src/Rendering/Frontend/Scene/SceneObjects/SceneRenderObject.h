#pragma once

#include "Defines.h"

#include "Rendering/Frontend/ResourceManager/ResourceObjects.h"

namespace Gecko {

	class SceneRenderObject
	{
	public:
		SceneRenderObject() = default;

		inline const MeshHandle& GetMeshHandle() const { return m_MeshHandle; };
		inline const MaterialHandle& GetMaterialHandle() const { return m_MaterialHandle; };

		inline void SetMeshHandle(MeshHandle meshHandle) { m_MeshHandle = meshHandle; };
		inline void SetMaterialHandle(MaterialHandle materialHandle) { m_MaterialHandle = materialHandle; };

	private:
		MeshHandle m_MeshHandle{ 0 };
		MaterialHandle m_MaterialHandle{ 0 };

	};

}