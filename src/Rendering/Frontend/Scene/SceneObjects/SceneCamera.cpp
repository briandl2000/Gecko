#include "Rendering/Frontend/Scene/SceneObjects/SceneCamera.h"

#include "Core/Platform.h"

namespace Gecko {

	SceneCamera::SceneCamera(ProjectionType type)
		: Camera(type)
	{
		
	}

	void SceneCamera::SetFieldOfView(f32 fieldOfView)
	{
		Camera::SetFieldOfView(fieldOfView);
	}

	void SceneCamera::SetAspectRatio(f32 aspectRatio)
	{
		Camera::SetAspectRatio(aspectRatio);
	}

	void SceneCamera::SetNear(f32 near)
	{
		Camera::SetNear(near);
	}

	void SceneCamera::SetFar(f32 far)
	{
		Camera::SetFar(far);
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

	void SceneCamera::SetTransform(const Transform& transform)
	{
		m_Transform = transform;
	}
}