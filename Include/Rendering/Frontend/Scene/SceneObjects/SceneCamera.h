#pragma once

#include "Defines.h"

#include "glm/common.hpp"

namespace Gecko {

	class SceneCamera
	{
	public:
		SceneCamera();

		inline f32 GetFieldOfView() const { return m_FieldOfView; }
		inline f32 GetAspectRatio() const { return m_AspectRatio; }
		inline f32 GetNear() const { return m_Near; }
		inline f32 GetFar() const { return m_Far; }
		inline bool IsAutoAspectRatio() const { return m_AutoAspectRatio; }
		inline bool IsMainCamera() const { return m_IsMain; }
		inline const glm::mat4& GetCachedProjection() const { return m_CachedProjectionMatrix; }

		void SetFieldOfView(f32 fieldOfView);
		void SetAspectRatio(f32 aspectRatio);
		void SetNear(f32 near);
		void SetFar(f32 far);
		void SetAutoAspectRatio(bool autoAspectRatio);
		void SetIsMain(bool isMain);

	private:
		void UpdateProjection();

	private:
		bool m_AutoAspectRatio{ false };
		bool m_IsMain{ false };

		f32 m_FieldOfView{ 90.f };
		f32 m_AspectRatio{ 1.f };
		f32 m_Near{ 0.1f };
		f32 m_Far{ 100.f };

		glm::mat4 m_CachedProjectionMatrix{ 1.f };
	};

}