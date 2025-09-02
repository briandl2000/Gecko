#include "Defines.h"

#include "Rendering/Frontend/ApplicationContext.h"

#include "Core/Input.h"

#include "Rendering/Backend/CommandList.h"
#include "Rendering/Frontend/Scene/Scene.h"
#include "Rendering/Frontend/Scene/SceneObjects/Transform.h"
#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"

class SceneExample : public Gecko::Scene
{
public:
	SceneExample() = default;
	virtual ~SceneExample() 
	{

	}

	virtual void Init(const std::string& name) override
	{

	}

	// Adjust aspect ratios of cameras
	virtual bool OnResize(const Gecko::Event::EventData& data) override
	{
		return false;
	}

protected:
	virtual const void PopulateSceneRenderInfo(Gecko::SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const
	{

	}

private:
};


struct float3 {
	union {
		struct {
			float x;
			float y;
			float z;
		} pos;
		struct {
			float r;
			float g;
			float b;
		} col;
		float arr[3];
	};
};

struct Vertex
{
	float3 position;
	float3 color;

	static const Gecko::VertexLayout GetLayout() {
		Gecko::VertexLayout layout({
			{ Gecko::DataFormat::R32G32B32_FLOAT, "POSITION"},
			{ Gecko::DataFormat::R32G32B32_FLOAT, "COLOR"}
		});
		return layout;
	}
};

Gecko::MeshHandle MeshHandle;

class TrianglePass : public Gecko::RenderPass<TrianglePass>
{
public:
	struct ConfigData : public Gecko::BaseConfigData
	{};

	TrianglePass() = default;
	virtual ~TrianglePass() {}
	virtual const void Render(const Gecko::SceneRenderInfo& sceneRenderInfo, Gecko::ResourceManager* resourceManager,
	const Gecko::Renderer* renderer, Gecko::Ref<Gecko::CommandList> commandList) override
	{
		Gecko::RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);
		Gecko::GraphicsPipeline pipeline = resourceManager->GetGraphicsPipeline(pipelineHandle);

		commandList->ClearRenderTarget(outputTarget);

		commandList->BindGraphicsPipeline(pipeline);
		commandList->BindRenderTarget(outputTarget);

		Gecko::Mesh& mesh = resourceManager->GetMesh(MeshHandle);
		commandList->BindVertexBuffer(mesh.VertexBuffer);
		commandList->BindIndexBuffer(mesh.IndexBuffer);

		commandList->Draw(3);
	}


protected:
	friend class Gecko::RenderPass<TrianglePass>;
	// Called by RenderPass<GeometryPass> to initialise this object with config data
	virtual const void SubInit(const Gecko::Platform::AppInfo& appInfo, Gecko::ResourceManager* resourceManager, const ConfigData& dependencies)
	{
		{	
			Gecko::GraphicsPipelineDesc pipelineDesc;
			pipelineDesc.VertexShaderPath = "Shaders/triangle.gsh";
			pipelineDesc.PixelShaderPath = "Shaders/triangle.gsh";
			pipelineDesc.ShaderVersion = "5_1";

			pipelineDesc.VertexLayout = Vertex::GetLayout();

			pipelineDesc.PipelineResources = {};
			pipelineDesc.NumRenderTargets = 1;
			pipelineDesc.RenderTextureFormats[0] = Gecko::DataFormat::R8G8B8A8_UNORM;

			pipelineDesc.WindingOrder = Gecko::WindingOrder::CounterClockWise;
			pipelineDesc.CullMode = Gecko::CullMode::Back;
			pipelineDesc.PrimitiveType = Gecko::PrimitiveType::Triangles;

			pipelineDesc.SamplerDescs = {
				{Gecko::ShaderType::Pixel, Gecko::SamplerFilter::Linear},
				{Gecko::ShaderType::Pixel, Gecko::SamplerFilter::Point}
			};

			pipelineHandle = resourceManager->CreateGraphicsPipeline(pipelineDesc);
		}


		Gecko::RenderTargetDesc renderTargetDesc;
		renderTargetDesc.Width = appInfo.Width;
		renderTargetDesc.Height = appInfo.Height;
		renderTargetDesc.NumRenderTargets = 1;
		renderTargetDesc.RenderTargetClearValues[0].Values[0] = 0.f;
		renderTargetDesc.RenderTargetClearValues[0].Values[1] = 0.f;
		renderTargetDesc.RenderTargetClearValues[0].Values[2] = 0.f;
		renderTargetDesc.RenderTargetClearValues[0].Values[3] = 0.f;
		renderTargetDesc.RenderTargetFormats[0] = Gecko::DataFormat::R8G8B8A8_UNORM; // color

		m_OutputHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "render target", true);
	}

private:
	Gecko::GraphicsPipelineHandle pipelineHandle;
};

int main()
{

	Gecko::Platform::AppInfo info;
	info.Width = 900;
	info.Height = 600;

	info.FullScreenWidth = 1920;
	info.FullScreenHeight = 1080;
	info.X = 200;
	info.Y = 200;
	info.NumBackBuffers = 2;
	info.Name = "Gecko App";
	info.WorkingDir = WORKING_DIR_PATH;

	Gecko::Event::Init();
	Gecko::Platform::Init(info);
	Gecko::Input::Init();
	Gecko::Logger::Init();

	// Create the context
	Gecko::ApplicationContext ctx;
	ctx.Init(info);

	Gecko::Renderer* renderer = ctx.GetRenderer();
	Gecko::ResourceManager* resourceManager = ctx.GetResourceManager();

	// Create the render passess
	Gecko::RenderPassHandle trianglePass = renderer->CreateRenderPass<TrianglePass>("Triangle");
	
	// Configure renderpasses
	renderer->ConfigureRenderPasses({
		trianglePass,
		});

	// Main scene initialisation
	Gecko::SceneManager* sceneManager = ctx.GetSceneManager();
	Gecko::u32 mainSceneIdx = sceneManager->CreateScene<SceneExample>("Main Scene");
	SceneExample* mainScene = sceneManager->GetScene<SceneExample>(mainSceneIdx);

	Gecko::f32 lastTime = Gecko::Platform::GetTime();

	Vertex triangleVertices[] = {
		{ { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
	};
	Gecko::u32 indices[] = { 0, 1, 2 };

	// Create the Vertex3D and index buffers
	Gecko::VertexBufferDesc vertexDesc;
	vertexDesc.Layout = Vertex::GetLayout();
	vertexDesc.NumVertices = static_cast<Gecko::u32>(3);
	vertexDesc.MemoryType = Gecko::MemoryType::Dedicated;
	vertexDesc.CanReadWrite = true;

	Gecko::IndexBufferDesc indexDesc;
	indexDesc.IndexFormat = Gecko::DataFormat::R32_UINT;
	indexDesc.NumIndices = static_cast<Gecko::u32>(3);
	indexDesc.MemoryType = Gecko::MemoryType::Dedicated;

	MeshHandle = resourceManager->CreateMesh(vertexDesc, indexDesc, triangleVertices, indices);

	while (Gecko::Platform::IsRunning()) {
		Gecko::Platform::PumpMessage();

		Gecko::f32 currentTime = Gecko::Platform::GetTime();
		Gecko::f32 deltaTime = (currentTime - lastTime);
		lastTime = currentTime;
				
		renderer->RenderScene(mainScene->GetSceneRenderInfo());

		Gecko::Input::Update();
	}

	ctx.Shutdown();

	Gecko::Logger::Shutdown();
	Gecko::Input::Shutdown();
	Gecko::Platform::Shutdown();
	Gecko::Event::Shutdown();

	return 0;
}