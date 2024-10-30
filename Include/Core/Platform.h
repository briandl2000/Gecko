#pragma once
#include "Defines.h"


namespace Gecko { namespace Platform 
{

	typedef void (*ResizeEvent)(u32 width, u32 height, void* listener);

	struct AppInfo
	{
		u32 Width = 1920;
		u32 Height = 1080;
		u32 FullScreenWidth = 1920;
		u32 FullScreenHeight = 1080;
		i32 X = 0;
		i32 Y = 0;
		u32 NumBackBuffers = 2;

		std::string Name = "Gecko";
		std::string WorkingDir = "";
	};

	bool Init(AppInfo& desc);
	void Shutdown();
	const AppInfo& GetAppInfo();
	void* GetWindowData();
	bool PumpMessage();
	bool IsRunning();
	float GetScreenAspectRatio();
	float GetTime();
	void AddResizeEvent(ResizeEvent resizeEvent, void* listener);

	// TODO: memory tracking
	void* CustomAllocate(size_t size);
	void* CustomRealloc(void* mem, size_t size);
	void CustomFree(void* mem);
	std::string GetLocalPath(std::string filePath);


} }