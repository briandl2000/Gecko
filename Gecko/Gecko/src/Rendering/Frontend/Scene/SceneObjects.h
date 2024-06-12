#pragma once

#include "Defines.h"
#include "Rendering/Frontend/ResourceManager/ResourceObjects.h"

#include "glm/common.hpp"

namespace Gecko {
	
struct NodeTransform
{
	glm::vec3 Position{ 0.f };
	glm::vec3 Rotation{ 0.f };
	glm::vec3 Scale{ 1.f };

	glm::mat4 GetMat4() const
	{
		glm::mat4 nodeTranslationMatrix = glm::translate(glm::mat4(1.), Position);
		glm::mat4 nodeRotationMatrix = glm::toMat4(glm::quat(Rotation));
		glm::mat4 nodeScaleMatrix = glm::scale(glm::mat4(1.), Scale);

		return nodeTranslationMatrix* nodeRotationMatrix* nodeScaleMatrix;
	}
};

struct MeshInstance {
	MeshHandle MeshHandle;
	MaterialHandle MaterialHandle;
};

enum class LightType
{
	Directional = 0,
	Point,
	Spot
};

struct Light
{
	LightType Type = LightType::Directional;

	glm::vec3 Color{ 1. };
	f32 Intensity{ 1. };

	union LightTypes
	{
		struct Point
		{
			f32 Radius;
		} Point;
		struct Spot
		{
			f32 Radius;
			f32 Angle;
		} Spot;
	};
};

struct Camera
{
	f32 FieldOfFiew{ 90.f };
	f32 Aspact{ 1.f };
	f32 Near{ 0.1f };
	f32 Far{ 100.f };
	bool keepAspect{ true };
};

struct MeshInstanceDescriptor
{
	MeshInstance MeshInstance;
	u32 NodeTransformMatrixIndex;
};

struct CameraDescriptor
{
	u32 ViewMatrixIndex;
	glm::mat4 Projection;
};

struct LightDescriptor
{
	Light Light;
	u32 NodeTransformMatrixIndex;
};

struct SceneDescriptor
{
	std::vector<glm::mat4> NodeTransformMatrices;
	std::vector<MeshInstanceDescriptor> Meshes;
	std::vector<LightDescriptor> lights;
	bool HasCamera{ false };
	CameraDescriptor Camera;
	EnvironmentMapHandle EnvironmentMap;
	TLAS* TLAS;
};

}