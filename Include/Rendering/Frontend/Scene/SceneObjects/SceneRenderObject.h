#pragma once

#include "Defines.h"
#include "Transform.h"
#include "Rendering/Frontend/Scene/SceneRenderInfo.h"

#include "Rendering/Frontend/ResourceManager/ResourceObjects.h"

namespace Gecko {

	class SceneRenderObject
	{
	public:
		SceneRenderObject() = default;

		inline const MeshHandle& GetMeshHandle() const { return m_MeshHandle; };
		inline const Material& GetMaterial() const { return m_Material; };
		inline const Transform& GetTransform() const { return m_Transform; }
		inline Transform& GetModifiableTransform() { return m_Transform; }

		inline void SetMeshHandle(MeshHandle meshHandle) { m_MeshHandle = meshHandle; };
		inline void SetMaterial(const Material& material) { m_Material = material; };
		inline void SetTransform(const Transform& transform) { m_Transform = transform; }

	private:
		MeshHandle m_MeshHandle{ 0 };
		Material m_Material{ 0 };

		Transform m_Transform{ Transform() };
	};

}