#pragma once

#ifdef WIN32
#pragma warning( push )
#pragma  warning(disable : 4201)
#endif

#define  GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#ifdef WIN32
#pragma warning( pop )
#endif

#include <memory>
#include <string>
#include <vector>


namespace Gecko
{

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t ;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double ;

#if defined(__clang__) || defined(__gcc__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

STATIC_ASSERT(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
STATIC_ASSERT(sizeof(u16) == 2, "Expected u16 to be 2 byte.");
STATIC_ASSERT(sizeof(u32) == 4, "Expected u32 to be 4 byte.");
STATIC_ASSERT(sizeof(u64) == 8, "Expected u64 to be 8 byte.");

STATIC_ASSERT(sizeof(i8) == 1, "Expected i8 to be 1 byte.");
STATIC_ASSERT(sizeof(i16) == 2, "Expected i16 to be 2 byte.");
STATIC_ASSERT(sizeof(i32) == 4, "Expected i32 to be 4 byte.");
STATIC_ASSERT(sizeof(i64) == 8, "Expected i64 to be 8 byte.");

STATIC_ASSERT(sizeof(f32) == 4, "Expected f32 to be 4 byte.");
STATIC_ASSERT(sizeof(f64) == 8, "Expected f64 to be 8 byte.");

#define BIT(x) (1 << x)

#ifdef DEBUG
#define GECKO_ENABLE_ASSERTS 1
#endif

	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	constexpr Scope<T> CreateScopeFromRaw(T* t)
	{
		return std::unique_ptr<T>(t);
	}

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	constexpr Ref<T> CreateRefFromRaw(T* t)
	{
		return std::shared_ptr<T>(t);
	}

	template<typename T>
	using WeakRef = std::weak_ptr<T>;

	template<typename T>
	constexpr WeakRef<T> CreateWeakRef(Ref<T> ref)
	{
		return std::weak_ptr<T>(ref);
	}
}