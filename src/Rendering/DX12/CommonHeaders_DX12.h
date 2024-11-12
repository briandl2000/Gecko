#ifdef DIRECTX_12
#pragma once

#include "Defines.h"

#include "Core/Asserts.h"
#include "Core/Logger.h"

#include "Rendering/DX12/d3dx12.h"
#include <dxgi1_6.h>
#include <d3d12.h>
#ifdef DEBUG
#include "dxgidebug.h"
#endif

#include <wrl.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#ifdef DEBUG
#pragma comment(lib, "dxguid.lib")
#endif

#ifdef DEBUG
#define DIRECTX12_ASSERT(hr) ASSERT(!FAILED(hr))
#define DIRECTX12_ASSERT_MSG(hr, message) ASSERT_MSG(!FAILED(hr), message)
#else
#define DIRECTX12_ASSERT(hr) hr
#define DIRECTX12_ASSERT_MSG(hr, message) hr
#endif
	
// TODO: Make use of custom string formating and wchar string class
#ifdef DEBUG
#define NAME_DIRECTX12_OBJECT(obj, name)			\
{													\
	const size_t cSize = strlen(name)+1;			\
	wchar_t* wc = new wchar_t[cSize];				\
	size_t outSize;									\
	mbstowcs_s(&outSize, wc, cSize, name, cSize-1);	\
	obj->SetName(wc);								\
	delete[] wc;									\
	LOG_DEBUG("D3D12 Object Created: %s", name);	\
}

#define NAME_DIRECTX12_OBJECT_INDEXED(obj, n, name)			\
{															\
	wchar_t outName[128];									\
	const size_t cSize = strlen(name)+1;					\
	wchar_t* wc = new wchar_t[cSize];						\
	size_t outSize;											\
	mbstowcs_s(&outSize, wc, cSize, name, cSize-1);			\
	if(swprintf_s(outName, L"%s[%u]", wc, n) > 0)			\
	{														\
		obj->SetName(outName);								\
		LOG_DEBUG("D3D12 Object Created: %s[%u]", name, n);	\
	}														\
	delete[] wc;											\
}
#else
#define NAME_DIRECTX12_OBJECT(obj, name)
#define NAME_DIRECTX12_OBJECT_INDEXED(obj, n, name)
#endif

#ifndef DISABLE_COPY
#define DISABLE_COPY(T)					\
		explicit T(const T&) = delete;	\
		T& operator=(const T&) = delete;
#endif

#ifndef DISABLE_MOVE
#define DISABLE_MOVE(T)				\
		explicit T(T&&) = delete;	\
		T& operator=(T&&) = delete;
#endif

#ifndef DISABLE_COPY_AND_MOVE
#define DISABLE_COPY_AND_MOVE(T) DISABLE_COPY(T) DISABLE_MOVE(T)
#endif

#endif // WIN32