#include "Core/Platform.h"

#if defined(WIN32)
#pragma warning( push )
#pragma warning(disable : 4018)
#pragma warning(disable : 4267)
#endif

#define STBI_MALLOC(sz) Gecko::Platform::CustomAllocate(sz)
#define STBI_REALLOC(p,newsz) Gecko::Platform::CustomRealloc(p,newsz)
#define STBI_FREE(p) Gecko::Platform::CustomFree(p)

#define TINYGLTF_USE_CPP14 
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <tiny_gltf.h>

#if defined(WIN32)
#pragma warning( pop )
#endif