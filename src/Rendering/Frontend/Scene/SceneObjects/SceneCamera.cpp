#include "Rendering/Frontend/Scene/SceneObjects/SceneCamera.h"

#include "Platform/Platform.h"

namespace Gecko {

	SceneCamera::SceneCamera()
	{
		UpdateProjection();
	}

	void SceneCamera::SetFieldOfView(f32 fieldOfView)
	{
		m_FieldOfView = fieldOfView;
		UpdateProjection();
	}

	void SceneCamera::SetAspectRatio(f32 aspectRatio)
	{
		m_AspectRatio = aspectRatio;
		UpdateProjection();
	}

	void SceneCamera::SetNear(f32 near)
	{
		m_Near = near;
		UpdateProjection();
	}

	void SceneCamera::SetFar(f32 far)
	{
		m_Far = far;
		UpdateProjection();
	}

	void SceneCamera::SetAutoAspectRatio(bool autoAspectRatio)
	{
		m_AutoAspectRatio = autoAspectRatio;
		if (m_AutoAspectRatio)
		{
			SetAspectRatio(Platform::GetScreenAspectRatio());
		}
	}

	void SceneCamera::SetIsMain(bool isMain)
	{ 
		m_IsMain = isMain; 
	}

	void SceneCamera::UpdateProjection()
	{
		m_CachedProjectionMatrix = glm::perspective(
			glm::radians(m_FieldOfView),
			m_AspectRatio,
			m_Near,
			m_Far
		);;
	}

}