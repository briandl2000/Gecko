#pragma once
#include "Defines.h"

namespace Gecko
{

enum class ProjectionType : u8
{
	None = 0,
	Perspective,
	Orthographic,
};

struct Camera
{
	union
	{
		struct
		{
			f32 FieldOfView;
			f32 AspectRatio;
		} Perspective;
		struct
		{
			f32 Width;
			f32 Height;
		} Orthographic;
	};

	f32 Near{ 0.1f };
	f32 Far{ 100.f };

	glm::mat4 CachedProjectionMatrix{ 1.f };
	
	ProjectionType Type{ ProjectionType::None };

	Camera(ProjectionType type = ProjectionType::None)
		: Type(type)
	{
		switch (type)
		{
		case ProjectionType::None:
			Perspective.FieldOfView = 0.f;
			Perspective.AspectRatio = 1.f;
			break;
		case ProjectionType::Perspective:
			Perspective.FieldOfView = 90.f;
			Perspective.AspectRatio = 1.f;
			break;
		case ProjectionType::Orthographic:
			Orthographic.Width = 10.f;
			Orthographic.Height = 10.f;
			break;
		}

		UpdateProjection();
	}

	bool IsValid() const
	{
		return Type != ProjectionType::None;
	}
	operator bool() const
	{
		return IsValid();
	}

	void SetFieldOfView(f32 fieldOfView)
	{
		Perspective.FieldOfView = fieldOfView;
		UpdateProjection();
	}

	void SetAspectRatio(f32 aspectRatio)
	{
		Perspective.AspectRatio = aspectRatio;
		UpdateProjection();
	}

	void SetWidth(f32 width)
	{
		Orthographic.Width = width;
		UpdateProjection();
	}

	void SetHeight(f32 height)
	{
		Orthographic.Height = height;
		UpdateProjection();
	}

	void SetNear(f32 near)
	{
		Near = near;
		UpdateProjection();
	}

	void SetFar(f32 far)
	{
		Far = far;
		UpdateProjection();
	}

private:
	void UpdateProjection()
	{
		switch (Type)
		{
		case ProjectionType::Perspective:
			CachedProjectionMatrix = glm::perspective(
				glm::radians(Perspective.FieldOfView),
				Perspective.AspectRatio,
				Near,
				Far
			);
			break;
		case ProjectionType::Orthographic:
			CachedProjectionMatrix = glm::ortho(
				-Orthographic.Width,
				Orthographic.Width,
				-Orthographic.Height,
				Orthographic.Height,
				Near,
				Far
			);
			break;
		default:
			return;
		}
	}
};

}
