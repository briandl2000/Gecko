#pragma once

#include "Defines.h"
#include "Transform.h"

#include "glm/common.hpp"

namespace Gecko {

	enum class LightType : u8
	{
		None = 0,
		Directional,
		Point,
		Spot
	};

	class SceneLight
	{
	public:
		SceneLight() = default;
		virtual ~SceneLight() {};

		inline LightType GetLightType() const { return m_LightType; }

		inline f32 GetIntenstiy() const { return m_Intensity; }
		inline const glm::vec3& GetColor() const { return m_Color; }
		inline const Transform& GetTransform() const { return m_Transform; }
		inline Transform& GetModifiableTransform() { return m_Transform; }

		inline void SetIntenstiy(f32 intensity) { m_Intensity = intensity; }
		inline void SetColor(glm::vec3 color) { m_Color = color; }
		inline void SetTransform(const Transform& transform) { m_Transform = transform; }

	protected:
		LightType m_LightType{ LightType::None };

		f32 m_Intensity{ 1.f };
		glm::vec3 m_Color{ 1.f };

		Transform m_Transform{ Transform() };
	};

	class SceneDirectionalLight : public SceneLight
	{
	public:
		SceneDirectionalLight() { m_LightType = LightType::Directional; };
		
	};

	class ScenePointLight : public SceneLight
	{
	public:
		ScenePointLight() { m_LightType = LightType::Point; }

		inline f32 GetRadius() const { return m_Radius; };

		inline void SetRadius(f32 radius) { m_Radius = radius; }

	private:
		f32 m_Radius{ 100.f };
	};


	class SceneSpotLight : public SceneLight
	{
	public:
		SceneSpotLight() { m_LightType = LightType::Spot; };

		inline f32 GetRadius() const { return m_Radius; };
		inline f32 GetAngle() const { return m_SpotAngle; };

		inline void SetRadius(f32 radius) { m_Radius = radius; }
		inline void SetSpotAngle(f32 angle) { m_SpotAngle = angle; }

	private:
		f32 m_Radius{ 100.f };
		f32 m_SpotAngle{ 45.f };
	};

}