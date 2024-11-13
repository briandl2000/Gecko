#pragma once

#include <glm/common.hpp>
#include "Defines.h"

namespace Gecko
{

class Transform
{
public:
	Transform(const glm::vec3& pos = glm::vec3(0.f), const glm::vec3& rot = glm::vec3(0.f), const glm::vec3& scale = glm::vec3(1.f))
		: Position(pos)
		, Rotation(rot)
		, Scale(scale)
	{}

	glm::vec3 Position{ 0.f };
	glm::vec3 Rotation{ 0.f };
	glm::vec3 Scale{ 1.f };

	inline glm::mat4 GetMat4() const
	{
		glm::mat4 nodeTranslationMatrix = glm::translate(glm::mat4(1.), Position);
		glm::mat4 nodeRotationMatrix = glm::toMat4(glm::quat(glm::radians(Rotation)));
		glm::mat4 nodeScaleMatrix = glm::scale(glm::mat4(1.), Scale);

		return nodeTranslationMatrix * nodeRotationMatrix * nodeScaleMatrix;
	}
};

}
