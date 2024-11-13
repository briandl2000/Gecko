#pragma once

#include "Defines.h"
#include "Transform.h"
#include "Camera.h"

#include "glm/common.hpp"

namespace Gecko {

	class SceneCamera : private Camera
	{
	public:
		SceneCamera(ProjectionType type);

		inline f32 GetFieldOfView() const { return Perspective.FieldOfView; }
		inline f32 GetAspectRatio() const { return Perspective.AspectRatio; }
		inline f32 GetNear() const { return Near; }
		inline f32 GetFar() const { return Far; }
		inline bool IsAutoAspectRatio() const { return m_AutoAspectRatio; }
		inline bool IsMainCamera() const { return m_IsMain; }
		inline const glm::mat4& GetCachedProjection() const { return CachedProjectionMatrix; }
		inline const Transform& GetTransform() const { return m_Transform; }
		inline Transform& GetModifiableTransform() { return m_Transform; }

		// Camera::Set functions are inherited privately, so we redeclare them publicly here
		void SetFieldOfView(f32 fieldOfView);
		void SetAspectRatio(f32 aspectRatio);
		void SetNear(f32 near);
		void SetFar(f32 far);

		void SetAutoAspectRatio(bool autoAspectRatio);
		void SetIsMain(bool isMain);
		void SetTransform(const Transform& transform);

	private:
		bool m_AutoAspectRatio{ false };
		bool m_IsMain{ false };

		Transform m_Transform{ Transform() };
	};

}