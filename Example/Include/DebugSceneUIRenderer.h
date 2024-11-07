#pragma once
#include "UI/DebugUIRenderer.h"

class DebugSceneUIRenderer : public Gecko::DebugUIRenderer
{
public:
	// Draw debug info of node-based scenes
	static bool RenderDebugUI(Gecko::ApplicationContext& ctx);
};
