#include "sound-engine/engine.h"
#include "impl/vulkan.h"
#include "glslang/Public/ShaderLang.h"

se::Engine::Engine()
{
	program = std::make_shared<GpuProgram>();
	auralizer = std::make_unique<Auralizer>(program);
}

se::Engine::~Engine() = default;

void se::Engine::Initialize()
{
	//glslang::InitializeProcess();
	program->Init();
	auralizer->TestInit();
}

void se::Engine::Shutdown()
{
	program->Destroy();
	//glslang::FinalizeProcess();
}
