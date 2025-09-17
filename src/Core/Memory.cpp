#include "Core/Memory.h"

#include "Core/Platform.h"

#if defined(WIN32)
#pragma warning( push )
#pragma warning(disable : 5040)
#endif

void* operator new (size_t size)
{
  return Gecko::Platform::CustomRealloc(nullptr, size);
}

void operator delete (void* data) throw()
{
  Gecko::Platform::CustomFree(data);
}

void* operator new[](std::size_t n)
{
  return Gecko::Platform::CustomRealloc(nullptr, n);
}
void operator delete[](void* p) throw()
{
  Gecko::Platform::CustomFree(p);
}

#if defined(WIN32)
#pragma warning( pop )
#endif

namespace Gecko { namespace Memory {



} }
