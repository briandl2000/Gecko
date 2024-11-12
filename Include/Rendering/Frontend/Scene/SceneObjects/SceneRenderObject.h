#pragma once

#include "Defines.h"
#include "Transform.h"

#include "Rendering/Frontend/ResourceManager/ResourceObjects.h"

namespace Gecko {

	class SceneRenderObject
	{
	public:
		SceneRenderObject() = default;

		inline const MeshHandle& GetMeshHandle() const { return m_MeshHandle; };
		inline const MaterialHandle& GetMaterialHandle() const { return m_MaterialHandle; };
		inline const Transform& GetTransform() const { return m_Transform; }
		inline Transform& GetModifiableTransform() { return m_Transform; }

		inline void SetMeshHandle(MeshHandle meshHandle) { m_MeshHandle = meshHandle; };
		inline void SetMaterialHandle(MaterialHandle materialHandle) { m_MaterialHandle = materialHandle; };
		inline void SetTransform(const Transform& transform) { m_Transform = transform; }

	private:
		MeshHandle m_MeshHandle{ 0 };
		MaterialHandle m_MaterialHandle{ 0 };

		Transform m_Transform{ Transform() };
	};

}